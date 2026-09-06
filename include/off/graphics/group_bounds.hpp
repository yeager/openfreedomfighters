#pragma once

#include "off/graphics/picture_bounds.hpp"
#include "off/graphics/position_update_service.hpp"

#include <optional>
#include <string_view>

namespace off::graphics {
struct ParentSpaceBounds {
    std::array<float,3> center;
    std::array<float,3> extents;
};
struct OwnerBounds {
    std::array<float,3> center;
    std::array<float,3> extents;
};
struct GroupBoundsChild {
    PositionResource* resource;
    bool owner_present;
};
struct GroupBoundsHooks {
    std::function<bool(const PositionResource&)> opt_out;
    std::function<std::optional<std::string_view>(const PositionResource&)> name;
    std::function<std::optional<ParentSpaceBounds>(PositionResource&)> get_bounds;
};

// Concrete group recomputation, not incremental expansion or recursive child
// initialization. Children are the actual ordered, stable runtime list. The
// getter supplies already eligible parent-space bounds, including its actual
// suppression/transform checks. Root status and parent presence are explicit.
class GroupBoundsRecompute final {
public:
    GroupBoundsRecompute() = default;
    GroupBoundsRecompute(const GroupBoundsRecompute&) = delete;
    GroupBoundsRecompute& operator=(const GroupBoundsRecompute&) = delete;
    GroupBoundsRecompute(GroupBoundsRecompute&&) = delete;
    GroupBoundsRecompute& operator=(GroupBoundsRecompute&&) = delete;

    // Native policies: nearest rounding, finite successful getter outputs with
    // nonnegative extents, stable acyclic live children/owners and no reentry.
    // Hooks must not destroy objects, change list/metadata or mutate group outputs;
    // getters may retain their declared external effects. All traversal and
    // arithmetic validation precede group writes. Unexpected failure poisons this
    // object, retaining hook effects but not inventing original rollback/retry.
    // Required hooks and child pointers are validated before invoking any hook.
    void apply(ResourceBounds& resource_bounds, std::uint32_t& runtime_flags,
               OwnerBounds& owner_bounds, std::span<const GroupBoundsChild> children,
               bool parent_owner_present, std::uint32_t owner_status,
               const GroupBoundsHooks& hooks);
    [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
    bool running_{false};
    bool failed_{false};
};
} // namespace off::graphics
