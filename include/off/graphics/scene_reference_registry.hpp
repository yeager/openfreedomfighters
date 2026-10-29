#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace off::graphics {

namespace detail {
// The caller owns this counter; the runtime registry uses a private shared
// counter and never reuses a token, even after failure or scene retirement.
inline std::uint32_t claim_scene_reference_token(std::atomic<std::uint32_t>& counter) {
    auto current=counter.load(std::memory_order_relaxed);
    for(;;) {
        if(current==0U || (current & 0x80000000U))
            throw std::runtime_error("scene reference token domain exhausted");
        if(counter.compare_exchange_weak(current,current-1U,std::memory_order_relaxed))
            return current;
    }
}
} // namespace detail

// Project-local references, not native handles. A lease identifies the exact
// registry generation supplying both relocation and resolution. No returned
// token alone conveys its owner, generation, component, camera, or view.
// State operations are synchronized with retirement. The registry object
// itself must remain alive until all concurrent method calls have returned.
class SceneReferenceRegistry final {
    struct State {
        std::mutex mutex;
        bool active{true};
        bool sealed{};
        std::uint64_t epoch{1};
        std::vector<std::uint32_t> rows;
        std::unordered_map<std::uint32_t,std::uint64_t> owners;
    };
public:
    class Lease final {
        friend class SceneReferenceRegistry;
        std::weak_ptr<State> state_;
        std::uint64_t epoch_{};
        Lease(const std::shared_ptr<State>& state, std::uint64_t epoch) : state_(state),epoch_(epoch) {}
    public:
        Lease() = default;
        [[nodiscard]] bool expired() const noexcept { return state_.expired(); }
        [[nodiscard]] bool stale() const {
            const auto state=state_.lock();
            if(!state) return true;
            std::lock_guard lock(state->mutex);
            return !state->active || !state->sealed || state->epoch!=epoch_;
        }
    };
    SceneReferenceRegistry() : state_(std::make_shared<State>()) {}
    SceneReferenceRegistry(const SceneReferenceRegistry&) = delete;
    SceneReferenceRegistry& operator=(const SceneReferenceRegistry&) = delete;
    ~SceneReferenceRegistry() { retire(); }

    // Call exactly once per successfully completed row, in source order.
    // A null owner is permitted only if the producer actually reached it.
    void append(std::optional<std::uint64_t> owner) {
        std::lock_guard lock(state_->mutex);
        if(!state_->active || state_->sealed) fail();
        if(!owner || *owner==0U) { state_->rows.push_back(0U); return; }
        const auto token=detail::claim_scene_reference_token(next_token_);
        const auto [entry,inserted]=state_->owners.emplace(token,*owner);
        if(!inserted) fail();
        try { state_->rows.push_back(token); }
        catch(...) { state_->owners.erase(entry); throw; }
    }
    [[nodiscard]] Lease seal() {
        std::lock_guard lock(state_->mutex);
        if(!state_->active || state_->sealed) fail();
        state_->sealed=true;
        return Lease(state_,state_->epoch);
    }
    // Identity-only check: a retired generation still belongs to its original
    // registry, so its queued entries can be discarded by that registry.
    [[nodiscard]] bool owns(const Lease& lease) const noexcept {
        const auto state=lease.state_.lock();
        return state && state==state_;
    }
    void retire() noexcept {
        if(state_) {
            std::lock_guard lock(state_->mutex);
            state_->active=false;
            state_->rows.clear();
            state_->owners.clear();
        }
    }
    [[nodiscard]] std::uint32_t relocate(std::uint32_t raw,const Lease& lease) const {
        const auto retained=checked_identity(lease);
        std::lock_guard lock(retained->mutex);
        const auto& s=checked_active(*retained,lease);
        const auto index=raw & 0x7fffffffU;
        if(index==0U) return raw;
        if(index>s.rows.size()) fail();
        return s.rows[index-1U];
    }
    [[nodiscard]] std::optional<std::uint64_t> resolve(std::uint32_t token,const Lease& lease) const {
        const auto retained=checked_identity(lease);
        std::lock_guard lock(retained->mutex);
        const auto& s=checked_active(*retained,lease);
        if(token==0U) return std::nullopt;
        const auto found=s.owners.find(token);
        if(found==s.owners.end()) fail();
        return found->second;
    }
private:
    [[nodiscard]] std::shared_ptr<State> checked_identity(const Lease& lease) const {
        const auto retained=lease.state_.lock();
        if(!retained || !state_ || retained.get()!=state_.get()) fail();
        return retained;
    }
    [[nodiscard]] static const State& checked_active(const State& state,const Lease& lease) {
        if(!state.active || !state.sealed || state.epoch!=lease.epoch_) fail();
        return state;
    }
    [[noreturn]] static void fail() { throw std::runtime_error("scene reference registry is unavailable or invalid"); }
    inline static std::atomic<std::uint32_t> next_token_{0x7ffffffeU};
    std::shared_ptr<State> state_;
};

} // namespace off::graphics
