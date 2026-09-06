#pragma once

#include "off/data/picture_resource.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace off::graphics {

struct PictureBounds {
    std::array<float, 3> center;
    std::array<float, 3> raw_extents;
    std::array<float, 3> extents;
    float radius;
};

struct ResourceBounds {
    std::array<float, 3> center;
    std::array<float, 3> extents;
    float radius;
};

// Concrete zero-identifier renderer-query path: no lookup or output writes.
// Caller proves retained ordinary runtime identity; nonzero identifiers are not
// supported by this adapter and reject rather than becoming guessed failures.
[[nodiscard]] bool query_zero_renderer_resource_bounds(
    std::uint64_t runtime_identity, std::uint64_t renderer_resource_id,
    std::array<float,3>& center, std::array<float,3>& extents);

class PictureBoundsApplication final {
public:
    using Query = std::function<bool(std::uint64_t, std::uint64_t,
                                    std::array<float,3>&, std::array<float,3>&)>;
    PictureBoundsApplication() = default;
    PictureBoundsApplication(const PictureBoundsApplication&) = delete;
    PictureBoundsApplication& operator=(const PictureBoundsApplication&) = delete;
    PictureBoundsApplication(PictureBoundsApplication&&) = delete;
    PictureBoundsApplication& operator=(PictureBoundsApplication&&) = delete;

    // Always query actual renderer-resource bounds before applying descriptor
    // bounds. Identifiers are explicit live identities, not authored PRM keys.
    // Descriptor computation is prevalidated before effects as a native policy;
    // the query must retain stable inputs/ownership and may only write its base
    // outputs. No reentry. Unexpected callback/returned-arithmetic failure retains
    // effects and poisons the application; no original rollback/retry is claimed.
    // No transform-cache, component-status or ancestor updates are performed.
    void apply(ResourceBounds& bounds, std::uint32_t& runtime_flags,
               std::uint64_t runtime_identity, std::uint64_t renderer_resource_id,
               std::span<const data::PictureResourceDescriptor> descriptors,
               std::span<const data::PictureDrawGroup> groups,
               std::array<float,2> scale, const Query& query);
    [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
    bool running_{false};
    bool failed_{false};
};

// Pure geometry computation, not the materialization callback: its preceding
// base initialization and ordered runtime writes remain separate operations.
// Group order/overlap is preserved; only referenced descriptors contribute.
// Native supported inputs: finite referenced XY centers/spans, nonnegative spans,
// positive finite scales, at least one visited descriptor and FE_TONEAREST.
// Descriptor Z/UV/color are not bounds inputs. Intermediate overflow rejects.
// Radius uses std::sqrt(double(binary32 squared sum)), narrowed to binary32
// before adding one; this is an explicit portable numerical policy, not a claim
// of cross-platform identity with the original square-root implementation.
[[nodiscard]] PictureBounds compute_picture_bounds(
    std::span<const data::PictureResourceDescriptor> descriptors,
    std::span<const data::PictureDrawGroup> groups,
    std::array<float, 2> scale);

} // namespace off::graphics
