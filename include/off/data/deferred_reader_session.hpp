#pragma once

#include "off/data/deferred_component_dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::data {

// The identity carried by one queued deferred reader work item.  It is kept
// independent from the graphics runtime so this boundary cannot accidentally
// turn a source offset or an allocator address into an owner identity.
struct DeferredReaderWorkIdentity final {
    std::uint64_t resource{};
    std::uint32_t source_offset{};
    std::size_t source_directory_index{};

    constexpr bool operator==(const DeferredReaderWorkIdentity&) const = default;
};

// The owner reader must explicitly nominate a suffix of the copied owner
// block and the exact component-block extent within that suffix.  The session
// does not infer either boundary from tags, attachment names, or a raw tail.
struct DeferredOwnerReaderResult final {
    std::span<const std::byte> component_suffix;
    std::size_t component_extent{};
};

using DeferredOwnerReader = std::function<DeferredOwnerReaderResult(std::span<const std::byte>)>;

enum class DeferredReaderSessionState : std::uint8_t {
    created,
    prepared,
    owner_read,
    component_read,
    deactivated,
};

// One-shot, fail-closed boundary for a single deferred owner record.  The
// complete source block is copied at admission.  Later callbacks may only
// return a suffix of that copy, so neither a foreign source span nor a prefix
// can be used to steer component dispatch.  Any callback or validation
// failure deactivates the session permanently.
class DeferredReaderSession final {
public:
    DeferredReaderSession(DeferredReaderWorkIdentity identity, std::span<const std::byte> complete_owner_block)
        : identity_(identity), owner_block_(complete_owner_block.begin(), complete_owner_block.end()) {
        if (owner_block_.empty()) {
            fail("deferred reader session requires one complete nonempty owner block");
        }
    }

    [[nodiscard]] DeferredReaderSessionState state() const noexcept { return state_; }
    [[nodiscard]] const DeferredReaderWorkIdentity& identity() const noexcept { return identity_; }
    [[nodiscard]] std::span<const std::byte> owner_block() const noexcept { return owner_block_; }

    void prepare(const DeferredReaderWorkIdentity& work) {
        require_state(DeferredReaderSessionState::created, "deferred reader session cannot be prepared twice");
        if (work != identity_) {
            fail("deferred reader session rejects foreign work identity");
        }
        state_ = DeferredReaderSessionState::prepared;
    }

    void read_owner(const DeferredOwnerReader& reader) {
        require_state(DeferredReaderSessionState::prepared,
                      "deferred reader session requires prepare before owner reading");
        if (!reader) {
            fail("deferred reader session requires an owner reader");
        }
        try {
            const auto result = reader(owner_block());
            validate_owner_result(result);
            component_suffix_ = result.component_suffix;
            component_extent_ = result.component_extent;
            state_ = DeferredReaderSessionState::owner_read;
        } catch (...) {
            deactivate_after_failure();
            throw;
        }
    }

    [[nodiscard]] DeferredComponentDispatchResult read_components(
        std::span<const DeferredComponentReader> attachment_readers) {
        require_state(DeferredReaderSessionState::owner_read,
                      "deferred reader session requires owner reading before component reading");
        try {
            const auto result = DeferredComponentDispatcher::dispatch(
                component_suffix_.first(component_extent_), attachment_readers);
            state_ = DeferredReaderSessionState::component_read;
            return result;
        } catch (...) {
            deactivate_after_failure();
            throw;
        }
    }

    void deactivate() {
        if (state_ == DeferredReaderSessionState::deactivated) {
            fail("deferred reader session cannot be deactivated twice");
        }
        component_suffix_ = {};
        component_extent_ = 0U;
        owner_block_.clear();
        owner_block_.shrink_to_fit();
        state_ = DeferredReaderSessionState::deactivated;
    }

private:
    void validate_owner_result(const DeferredOwnerReaderResult& result) const {
        if (result.component_suffix.empty()) {
            fail("deferred owner reader must provide a nonempty component suffix");
        }
        const auto owner_begin = reinterpret_cast<std::uintptr_t>(owner_block_.data());
        const auto owner_end = owner_begin + owner_block_.size();
        const auto suffix_begin = reinterpret_cast<std::uintptr_t>(result.component_suffix.data());
        const auto suffix_end = suffix_begin + result.component_suffix.size();
        if (suffix_begin < owner_begin || suffix_end < suffix_begin || suffix_end != owner_end) {
            fail("deferred owner reader returned a foreign or non-suffix component cursor");
        }
        if (result.component_extent == 0U || result.component_extent > result.component_suffix.size()) {
            fail("deferred owner reader must provide an explicit bounded component extent");
        }
    }

    void require_state(DeferredReaderSessionState expected, const char* message) const {
        if (state_ != expected) {
            throw std::runtime_error(message);
        }
    }

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }

    void deactivate_after_failure() noexcept {
        component_suffix_ = {};
        component_extent_ = 0U;
        owner_block_.clear();
        owner_block_.shrink_to_fit();
        state_ = DeferredReaderSessionState::deactivated;
    }

    DeferredReaderWorkIdentity identity_;
    std::vector<std::byte> owner_block_;
    std::span<const std::byte> component_suffix_;
    std::size_t component_extent_{};
    DeferredReaderSessionState state_{DeferredReaderSessionState::created};
};

} // namespace off::data
