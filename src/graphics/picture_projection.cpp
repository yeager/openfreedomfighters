#include "off/graphics/picture_projection.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
float checked(double value) {
  constexpr double limit = std::numeric_limits<float>::max();
  if (!std::isfinite(value) || value < -limit || value > limit)
    throw std::runtime_error(
        "Picture projection exceeds finite binary32 range");
  return static_cast<float>(value);
}
void finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Picture projection requires finite inputs");
}
} // namespace

PictureProjection prepare_picture_projection(float near_distance,
                                             float far_distance,
                                             float half_extent_0,
                                             float half_extent_1) {
  for (float value :
       {near_distance, far_distance, half_extent_0, half_extent_1})
    finite(value);
  if (near_distance < 5 || far_distance <= near_distance ||
      half_extent_0 == 0 || half_extent_1 == 0)
    throw std::runtime_error(
        "Picture projection requires resolved valid frustum inputs");
  const double n = near_distance;
  const double f = far_distance;
  PictureProjection result;
  result.matrix_[0] = checked(n / half_extent_0);
  result.matrix_[5] = checked(n / half_extent_1);
  result.matrix_[10] = checked(f / (f - n));
  result.matrix_[11] = 1;
  result.matrix_[14] = checked(-(f * n) / (f - n));
  return result;
}

std::array<float, 4>
project_picture_position(const PictureProjection &projection,
                         const std::array<float, 3> &position) {
  for (float value : position)
    finite(value);
  const auto &matrix = projection.matrix();
  // Materialize the product to avoid contraction with the depth offset.
  const volatile double depth_product = double(position[2]) * matrix[10];
  return {checked(double(position[0]) * matrix[0]),
          checked(double(position[1]) * matrix[5]),
          checked(depth_product + matrix[14]), position[2]};
}

std::array<float, 3>
map_picture_clip_to_viewport(const std::array<float, 4> &clip,
                             const PictureViewport &viewport) {
  for (float value : clip)
    finite(value);
  constexpr auto limit = std::numeric_limits<std::uint32_t>::max();
  if (viewport.width == 0 || viewport.height == 0 ||
      std::uint64_t(viewport.x) + viewport.width > limit ||
      std::uint64_t(viewport.y) + viewport.height > limit || clip[3] == 0)
    throw std::runtime_error(
        "Picture viewport requires nonzero W and bounded positive extent");
  const double w = clip[3];
  const volatile double x_offset =
      (1.0 + double(clip[0]) / w) * (double(viewport.width) * 0.5);
  const volatile double y_offset =
      (1.0 - double(clip[1]) / w) * (double(viewport.height) * 0.5);
  return {checked(double(viewport.x) + x_offset),
          checked(double(viewport.y) + y_offset), checked(double(clip[2]) / w)};
}

} // namespace off::graphics
