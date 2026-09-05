#pragma once

#include "off/graphics/picture_projection.hpp"

#include <array>

namespace off::graphics {

struct PicturePassRectangle {
  float left;
  float top;
  float right;
  float bottom;
};
struct PictureViewCommonInput {
  float raw_near;
  float selected_far;
  PicturePassRectangle rectangle;
  float renderer_dimension_0;
  float renderer_dimension_1;
  float virtual_aspect;
};
struct PictureOrdinaryCameraInput {
  float angle_radians;
  float renderer_scalar;
};
struct PictureAlternateCameraInput {
  float parameter_0;
  float parameter_1;
};
struct PictureResolvedView {
  float near_distance;
  float far_distance;
  float half_extent_0;
  float half_extent_1;
  PictureProjection projection;
};

// Explicit camera/view producer, not final startup camera selection. Finite
// inputs and intermediates are required; divisors must be nonzero. Near is
// bounded from below by 5. Only this projection helper substitutes 1 for zero
// rectangle dimensions; signed dimensions otherwise retain their sign.
// Equations use sequenced double intermediates, then checked binary32 resolved
// values and prepare_picture_projection's valid-frustum safety policy. This
// does not promise bit-exact retail floating-point evaluation.
[[nodiscard]] PictureResolvedView
prepare_picture_view_parameters(const PictureViewCommonInput &common,
                                const PictureOrdinaryCameraInput &camera);
[[nodiscard]] PictureResolvedView
prepare_picture_view_parameters(const PictureViewCommonInput &common,
                                const PictureAlternateCameraInput &camera);

// Raw viewport X/Y/Width/Height request before integer conversion/validation.
// Applies only lower bounds to a/b and upper bounds to c/d. Uses the ORIGINAL
// rectangle dimensions, including zero and signed values; no projection-only
// substitution, whole-range clamp, or target-size validation is performed.
[[nodiscard]] std::array<float, 4>
prepare_picture_viewport_request(const PicturePassRectangle &rectangle,
                                 const std::array<float, 4> &camera_viewport);

} // namespace off::graphics
