#pragma once

#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/picture_transform.hpp"

namespace off::graphics {

// Stored visitor rectangle, not an inferred drawable or texture size.
struct PictureVisitorRectangle {
  std::int32_t left;
  std::int32_t top;
  std::int32_t right;
  std::int32_t bottom;
};

struct PictureCameraServices {
  std::array<float, 4> viewport;
  std::array<float, 2> pass_dimensions;
  float projection_scalar;
};

// Ordinary-camera projection of view preparation only, not full frustum state
// or camera admission. Checks all inputs before clearing canonical runtime bit
// 0x2 as native failure policy. No enabled-state notification. Requires nearest
// rounding, positive signed32 rectangle extents and finite binary32 stages.
// Tangent uses std::tan(double(half_angle)), narrowed before multiplying far;
// this portable math-library policy does not emulate the original x87 library.
[[nodiscard]] PictureCameraServices prepare_picture_camera_services(
    CameraEnabledState& flags, const IntroCameraState& camera,
    const PictureVisitorRectangle& rectangle);

// Join for an admitted intro picture draw. Live hierarchy membership/transforms
// and prior bounds/Center/scale updates remain the scene owner's responsibility.
// The camera endpoint is its resource, not its selected window. Stored alignment
// has XY only. The undefined original Y operand is an explicit native policy
// argument; no fallback value is supplied. This does not mutate/invalidate the
// cache or establish visibility, registration, projection or material state.
[[nodiscard]] PictureCacheTransformInput make_intro_picture_cache_input(
    const std::vector<PictureHierarchyNode>& live_hierarchy,
    std::uint32_t picture_node, std::uint32_t camera_node,
    std::array<float, 2> stored_alignment, std::array<float, 2> picture_scale,
    const PictureCameraServices& camera, float external_y_basis_scale_policy);

} // namespace off::graphics
