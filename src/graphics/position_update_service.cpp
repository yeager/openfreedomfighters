#include "off/graphics/position_update_service.hpp"

#include <exception>
#include <stdexcept>

namespace off::graphics {
namespace {
struct Guard {
    bool& running;
    bool& failed;
    int exceptions{std::uncaught_exceptions()};
    Guard(bool& active, bool& poisoned) : running(active), failed(poisoned) { running = true; }
    ~Guard() {
        if (std::uncaught_exceptions() > exceptions) failed = true;
        running = false;
    }
};
}

void PositionUpdateService::validate_entry() const {
    if (running_ || failed_ || count_ > handles_.size())
        throw std::runtime_error("position service state or reentry is unsupported");
}
void PositionUpdateService::validate_flush(PositionServiceMode mode,
                                           const PositionServiceHooks& hooks) {
    if (!hooks.resolve || (mode.collection_enabled &&
        (!hooks.bounds || !hooks.maintenance || !hooks.final_batch)))
        throw std::runtime_error("position service flush hooks are required");
}
void PositionUpdateService::notify(PositionResource& resource, PositionServiceMode mode,
                                    const PositionServiceHooks& hooks) {
    validate_entry();
    if (mode.immediate) {
        if (!hooks.bounds) throw std::runtime_error("immediate position bounds hook is required");
        Guard guard(running_, failed_);
        if (hooks.spatial_change) hooks.spatial_change(resource);
        hooks.bounds(resource);
        return;
    }
    if (!mode.collection_enabled || (resource.flags & 0x20200000U) != 0U) return;
    if (!hooks.make_handle) throw std::runtime_error("position handle producer is required");
    if (count_ == handles_.size()) validate_flush(mode, hooks);
    Guard guard(running_, failed_);
    if (count_ == handles_.size()) flush_admitted(mode, hooks);
    // Deliberately do not repeat admission after a capacity flush.
    resource.flags |= 0x23000000U;
    const auto handle = hooks.make_handle(resource);
    handles_[count_] = handle;
    ++count_;
}
void PositionUpdateService::flush(PositionServiceMode mode, const PositionServiceHooks& hooks) {
    validate_entry();
    validate_flush(mode, hooks);
    Guard guard(running_, failed_);
    flush_admitted(mode, hooks);
}
void PositionUpdateService::flush_admitted(PositionServiceMode mode,
                                           const PositionServiceHooks& hooks) {
    std::array<PositionResource*, 50> survivors{};
    std::size_t survivor_count = 0;
    // Original loops use live count; no queue mutation/reentry makes this bound
    // equivalent. Resolve once, clear the queue bit, then retain live identity.
    for (std::size_t i = 0; i < count_; ++i) {
        if (auto* resource = hooks.resolve(handles_[i])) {
            resource->flags &= ~0x20000000U;
            survivors[survivor_count++] = resource;
        }
    }
    if (!mode.collection_enabled) {
        count_ = 0;
        return;
    }
    count_ = survivor_count;
    for (std::size_t i = 0; i < count_; ++i) {
        auto& resource = *survivors[i];
        if ((resource.flags & 0x200000U) != 0U) continue;
        if ((resource.flags & 0x40000U) != 0U) hooks.maintenance(resource);
        else if (mode.suppression <= 0) hooks.bounds(resource);
    }
    hooks.final_batch(std::span<PositionResource* const>(survivors.data(), survivor_count));
    count_ = 0;
}

} // namespace off::graphics
