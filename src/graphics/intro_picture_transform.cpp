#include "off/graphics/intro_picture_transform.hpp"

#include <cfenv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
float finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("intro picture camera arithmetic must be finite");
  return value;
}
float multiply(float a, float b) {
  const volatile float result = a * b;
  return finite(result);
}
float divide(float a, float b) {
  if (b == 0)
    throw std::runtime_error("intro picture camera divisor is zero");
  const volatile float result = a / b;
  return finite(result);
}
float narrow(double value) {
  constexpr double limit = std::numeric_limits<float>::max();
  if (!std::isfinite(value) || value < -limit || value > limit)
    throw std::runtime_error("intro picture camera value exceeds binary32 range");
  const volatile float result = static_cast<float>(value);
  return finite(result);
}
float signed_extent(std::int32_t lower, std::int32_t upper) {
  const auto difference = static_cast<std::int64_t>(upper) - lower;
  if (difference <= 0 || difference > std::numeric_limits<std::int32_t>::max())
    throw std::runtime_error("intro picture visitor extent must be positive signed32");
  const volatile float result = static_cast<float>(static_cast<std::int32_t>(difference));
  return result;
}
void nearest() {
  if (std::fegetround() != FE_TONEAREST)
    throw std::runtime_error("intro picture preparation requires nearest rounding");
}
// Explicit native numerical policy for the transpose-form basis join: each
// product and left-to-right sum is rounded to binary32, without FMA contraction.
float dot(float x, float y, float z, float a, float b, float c) {
  const auto xa = multiply(x, a);
  const auto yb = multiply(y, b);
  const auto zc = multiply(z, c);
  const volatile float first = xa + yb;
  const volatile float result = finite(first) + zc;
  return finite(result);
}
} // namespace

PictureCameraServices prepare_picture_camera_services(
    CameraEnabledState& flags, const IntroCameraState& camera,
    const PictureVisitorRectangle& rectangle) {
  nearest();
  if ((flags.flags() & 1U) != 0)
    throw std::runtime_error("intro picture services require the ordinary camera branch");
  for (float value : camera.viewport) finite(value);
  if (camera.viewport[2] == 0)
    throw std::runtime_error("intro picture camera viewport divisor is zero");
  const std::array dimensions{signed_extent(rectangle.left, rectangle.right),
                              signed_extent(rectangle.top, rectangle.bottom)};
  const auto half_angle = multiply(finite(camera.angle_radians), 0.5F);
  const auto tangent = narrow(std::tan(static_cast<double>(half_angle)));
  const auto far_extent = multiply(tangent, finite(camera.far_distance));
  const auto reciprocal = divide(camera.far_distance, far_extent);
  const auto scalar = multiply(reciprocal, 0.5F);
  if (scalar == 0)
    throw std::runtime_error("intro picture camera scalar divisor is zero");
  flags.clear_picture_preparation_bit();
  return {camera.viewport, dimensions, scalar};
}

PictureCacheTransformInput make_intro_picture_cache_input(
    const std::vector<PictureHierarchyNode>& live_hierarchy,
    std::uint32_t picture_node, std::uint32_t camera_node,
    std::array<float, 2> stored_alignment, std::array<float, 2> picture_scale,
    const PictureCameraServices& camera, float external_y_basis_scale_policy) {
  nearest();
  for (float value : stored_alignment) finite(value);
  for (float value : picture_scale)
    if (finite(value) < 0)
      throw std::runtime_error("intro picture scale must not be negative");
  for (float value : camera.viewport) finite(value);
  for (float value : camera.pass_dimensions)
    if (finite(value) <= 0)
      throw std::runtime_error("intro picture pass dimensions must be positive");
  if (camera.viewport[2] == 0 || finite(camera.projection_scalar) == 0)
    throw std::runtime_error("intro picture service divisor is zero");
  finite(external_y_basis_scale_policy);
  const auto relative = produce_picture_hierarchy_transform(live_hierarchy, picture_node, camera_node);
  const auto& orientation = live_hierarchy[picture_node].matrix;
  auto basis = relative.basis;
  for (std::size_t i = 0; i < basis.size(); i += 3) {
    const auto x = relative.basis[i], y = relative.basis[i + 1], z = relative.basis[i + 2];
    basis[i] = dot(x, y, z, orientation[6], orientation[7], orientation[8]);
    basis[i + 1] = dot(x, y, z, orientation[3], orientation[4], orientation[5]);
    basis[i + 2] = dot(x, y, z, orientation[0], orientation[1], orientation[2]);
  }
  return {.submission_position = relative.position,
          .aligned_local_position = {stored_alignment[0], stored_alignment[1], 0.0F},
          .virtual_window_scale = camera.viewport,
          .cached_basis = basis,
          .object_matrix = orientation,
          .viewport_width = camera.pass_dimensions[0],
          .viewport_height = camera.pass_dimensions[1],
          .picture_width = picture_scale[0],
          .picture_height = picture_scale[1],
          .owner_projection_scalar = camera.projection_scalar,
          .external_y_basis_scale = external_y_basis_scale_policy};
}

} // namespace off::graphics
