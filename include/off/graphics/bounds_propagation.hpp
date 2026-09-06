#pragma once

#include "off/graphics/group_bounds.hpp"

namespace off::graphics {
struct BoundsNode {
    PositionResource* resource;
    ResourceBounds* bounds;
    OwnerBounds* owner_bounds;
    BoundsNode* parent;
    std::array<float,9> basis;
    std::array<float,3> position;
    bool owner_opt_out;
};

// Explicit stable suppression and current local transform. Eligibility stops
// precede arithmetic validation; ignored fields/farther parents are not read.
// Supported consumed state is finite with nonnegative radius/extents and the
// established identity-basis storage convention. This is not a world transform.
[[nodiscard]] std::optional<ParentSpaceBounds> get_identity_parent_bounds(
    const BoundsNode& node, std::int32_t suppression);

class IdentityBoundsPropagation final {
public:
    IdentityBoundsPropagation() = default;
    IdentityBoundsPropagation(const IdentityBoundsPropagation&) = delete;
    IdentityBoundsPropagation& operator=(const IdentityBoundsPropagation&) = delete;
    IdentityBoundsPropagation(IdentityBoundsPropagation&&) = delete;
    IdentityBoundsPropagation& operator=(IdentityBoundsPropagation&&) = delete;

    // Expansion-only propagation over shared, live state. No child recomputation,
    // parent initialization, cache invalidation or position-service notification.
    // Stable links, suppression and owner opt-out results are explicit inputs.
    // Validate arithmetic and owner storage before each parent update. A later
    // unsupported node retains earlier updates and poisons this object; no retry
    // or rollback is inferred. Only reached cycles/unsupported state are checked.
    // Returns the number of updated parents; an eligibility/no-expansion stop
    // leaves farther ancestors untouched and uninspected.
    std::size_t apply(BoundsNode& changed, std::int32_t suppression);
    [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
    bool running_{false};
    bool failed_{false};
};
} // namespace off::graphics
