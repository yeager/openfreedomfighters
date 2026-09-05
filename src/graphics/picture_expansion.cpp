#include "off/graphics/picture_expansion.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::graphics {
namespace {

float finite_float(double value) {
  constexpr double limit = std::numeric_limits<float>::max();
  if (!std::isfinite(value) || value < -limit || value > limit)
    throw std::runtime_error("Picture expansion exceeds finite binary32 range");
  return static_cast<float>(value);
}

void require_finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Picture expansion requires finite inputs");
}

} // namespace

std::vector<ExpandedPictureBatch>
expand_picture_descriptors(std::span<const data::PictureQuad> descriptors,
                           const PictureCacheTransform &transform) {
  if (descriptors.size() > picture_expansion_limit)
    throw std::runtime_error("Picture expansion descriptor limit exceeded");
  for (float value : transform.basis)
    require_finite(value);
  for (float value : transform.translation)
    require_finite(value);
  const auto &m = transform.basis;
  const double horizontal_scale =
      std::hypot(double(m[6]), double(m[7]), double(m[8]));
  const double vertical_scale =
      std::hypot(double(m[3]), double(m[4]), double(m[5]));
  std::vector<ExpandedPictureBatch> batches;
  for (std::size_t first = 0; first < descriptors.size();
       first += picture_expansion_batch_limit) {
    const auto count =
        std::min(picture_expansion_batch_limit, descriptors.size() - first);
    ExpandedPictureBatch batch{
        .first_descriptor = first, .vertices = {}, .indices = {}};
    batch.vertices.reserve(count * 4);
    batch.indices.reserve(count * 6);
    for (std::size_t offset = 0; offset < count; ++offset) {
      const auto &quad = descriptors[first + offset];
      for (float value :
           {quad.local_center_x, quad.local_center_y, quad.local_z,
            quad.horizontal_edge_span, quad.vertical_edge_span, quad.u_min,
            quad.u_max, quad.v_min, quad.v_max})
        require_finite(value);
      if (quad.horizontal_edge_span < 0 || quad.vertical_edge_span < 0)
        throw std::runtime_error(
            "Picture expansion requires nonnegative spans");
      std::array<float, 3> center{};
      for (std::size_t axis = 0; axis < center.size(); ++axis) {
        // Materialize each double operation to prevent multiply-add
        // contraction.
        const volatile double x = double(quad.local_center_x) * m[6 + axis];
        const volatile double y = double(quad.local_center_y) * m[3 + axis];
        const volatile double z = double(quad.local_z) * m[axis];
        const volatile double xy = x + y;
        const float linear = finite_float(xy + z);
        center[axis] =
            finite_float(double(linear) + transform.translation[axis]);
      }
      const double half_width =
          double(quad.horizontal_edge_span) * 0.5 * horizontal_scale;
      const double half_height =
          double(quad.vertical_edge_span) * 0.5 * vertical_scale;
      const float left = finite_float(double(center[0]) - half_width);
      const float right = finite_float(double(center[0]) + half_width);
      const float low = finite_float(double(center[1]) - half_height);
      const float high = finite_float(double(center[1]) + half_height);
      const auto color = (quad.modulation_color >> 1U) & 0x7f7f7f7fU;
      batch.vertices.insert(
          batch.vertices.end(),
          {{{left, low, center[2]}, {quad.u_min, quad.v_max}, color},
           {{right, low, center[2]}, {quad.u_max, quad.v_max}, color},
           {{right, high, center[2]}, {quad.u_max, quad.v_min}, color},
           {{left, high, center[2]}, {quad.u_min, quad.v_min}, color}});
      const auto base = static_cast<std::uint16_t>(offset * 4);
      for (const unsigned corner : {0U, 1U, 3U, 1U, 2U, 3U})
        batch.indices.push_back(static_cast<std::uint16_t>(base + corner));
    }
    batches.push_back(std::move(batch));
  }
  return batches;
}

} // namespace off::graphics
