#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace off::graphics {

struct PositionResource {
    std::uint64_t identity;
    std::uint32_t flags;
};
struct PositionServiceMode {
    bool immediate;
    bool collection_enabled;
    std::int32_t suppression;
};
struct PositionServiceHooks {
    std::function<std::uint64_t(PositionResource&)> make_handle;
    std::function<PositionResource*(std::uint64_t)> resolve;
    std::function<void(PositionResource&)> spatial_change; // Optional.
    std::function<void(PositionResource&)> bounds; // No incoming child.
    std::function<void(PositionResource&)> maintenance;
    std::function<void(std::span<PositionResource* const>)> final_batch;
};

// Completed final-batch scene behavior for an entirely plain-picture batch.
// Owner class comes from live metadata, not PositionResource::flags. Validate
// every survivor, even hidden ones, as a stronger native admission policy.
// A pure, stable lookup and non-null live owners are required. Mixed/unknown
// classes reject; callers needing them must retain the broader batch handler.
// Once admitted, the original selection is empty and has no scene effects.
void complete_plain_picture_position_batch(
    std::span<PositionResource* const> survivors,
    const std::function<std::optional<std::uint32_t>(const PositionResource&)>& owner_class);

// Shared manager-level service, not a per-picture queue or scene admission gate.
// Handles are opaque; the supplied resolver implements actual validity checks.
class PositionUpdateService final {
public:
    PositionUpdateService() = default;
    PositionUpdateService(const PositionUpdateService&) = delete;
    PositionUpdateService& operator=(const PositionUpdateService&) = delete;
    PositionUpdateService(PositionUpdateService&&) = delete;
    PositionUpdateService& operator=(PositionUpdateService&&) = delete;

    // Caller retains live resources and stable mode/registry throughout each
    // operation. Hooks may change resource flags, but must not reenter, destroy
    // resources or change queue/registry/mode. Hooks must not throw on admitted
    // paths. Unexpected exceptions retain prior effects and poison this service;
    // further operations reject, rather than inventing original retry behavior.
    // Missing required hooks reject before effects and do not poison the service.
    void notify(PositionResource& resource, PositionServiceMode mode,
                const PositionServiceHooks& hooks);
    void flush(PositionServiceMode mode, const PositionServiceHooks& hooks);
    [[nodiscard]] std::size_t pending_count() const noexcept { return count_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }

private:
    void validate_entry() const;
    static void validate_flush(PositionServiceMode mode, const PositionServiceHooks& hooks);
    void flush_admitted(PositionServiceMode mode, const PositionServiceHooks& hooks);
    std::array<std::uint64_t, 50> handles_{};
    std::size_t count_{0};
    bool running_{false};
    bool failed_{false};
};

} // namespace off::graphics
