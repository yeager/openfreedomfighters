#include "off/graphics/picture_projection.hpp"

#include <cmath>
#include <bit>
#include <cfenv>
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

PictureViewport
convert_picture_viewport_request(const std::array<float, 4> &request) {
  const auto component = [](float value) -> std::uint32_t {
    // The exclusive positive endpoint is exact; converting INT64_MAX to
    // binary32 would round up and admit an undefined float-to-integer cast.
    if (!std::isfinite(value) || double(value) < -0x1p63 ||
        double(value) >= 0x1p63)
      throw std::runtime_error("Picture viewport request exceeds signed64 range");
    const auto truncated = static_cast<std::int64_t>(value);
    return static_cast<std::uint32_t>(truncated);
  };
  return {component(request[0]), component(request[1]), component(request[2]),
          component(request[3])};
}

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
  if(std::fegetround()!=FE_TONEAREST)
    throw std::runtime_error("Picture projection requires nearest-even rounding");
  const auto add=[](float a,float b) { const volatile float v=a+b; finite(v); return float(v); };
  const auto subtract=[](float a,float b) { const volatile float v=a-b; finite(v); return float(v); };
  const auto multiply=[](float a,float b) { const volatile float v=a*b; finite(v); return float(v); };
  const auto divide=[](float a,float b) {
    if(b==0) throw std::runtime_error("Picture projection divisor is zero");
    const volatile float v=a/b; finite(v); return float(v);
  };
  const auto negate=[](float v) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(v)^0x80000000U); };
  const float nx=negate(half_extent_0),ny=negate(half_extent_1);
  const float dx=subtract(half_extent_0,nx),dy=subtract(half_extent_1,ny);
  const float nn=add(near_distance,near_distance),d=subtract(far_distance,near_distance);
  PictureProjection result;
  result.matrix_[0] = divide(nn,dx);
  result.matrix_[5] = divide(nn,dy);
  result.matrix_[8] = divide(add(half_extent_0,nx),dx);
  result.matrix_[9] = divide(add(ny,half_extent_1),dy);
  result.matrix_[10] = divide(far_distance,d);
  result.matrix_[11] = 1;
  const float negative_far=multiply(far_distance,-1.0F);
  result.matrix_[14] = divide(multiply(negative_far,near_distance),d);
  return result;
}

PictureProjection prepare_picture_projection_alternate_depth(
    float near_distance, float half_extent_0, float half_extent_1) {
  for (float value : {near_distance, half_extent_0, half_extent_1})
    finite(value);
  if (near_distance < 5 || half_extent_0 == 0 || half_extent_1 == 0)
    throw std::runtime_error(
        "Alternate picture projection requires resolved valid inputs");
  const double n = near_distance;
  PictureProjection result;
  result.matrix_[0] = checked(n / half_extent_0);
  result.matrix_[5] = checked(n / half_extent_1);
  result.matrix_[10] = 1;
  result.matrix_[11] = 1;
  result.matrix_[14] = checked(-n);
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
