#include "off/graphics/picture_transform.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace off::graphics {
namespace {

constexpr float alignment_rounding_bias = 1.0F / 8192.0F;

template <std::size_t Size>
void require_finite(const std::array<float, Size> &values,
                    const char *message) {
  for (const auto value : values) {
    if (!std::isfinite(value))
      throw std::runtime_error(message);
  }
}

[[nodiscard]] std::array<float, 3>
transform_vector(const std::array<float, 3> &value,
                 const std::array<float, 9> &matrix) {
  return {
      value[0] * matrix[6] + value[1] * matrix[3] + value[2] * matrix[0],
      value[0] * matrix[7] + value[1] * matrix[4] + value[2] * matrix[1],
      value[0] * matrix[8] + value[1] * matrix[5] + value[2] * matrix[2],
  };
}

[[nodiscard]] std::array<float, 3>
inverse_transform_vector(const std::array<float, 3> &value,
                         const std::array<float, 9> &matrix) {
  return {
      value[0] * matrix[6] + value[1] * matrix[7] + value[2] * matrix[8],
      value[0] * matrix[3] + value[1] * matrix[4] + value[2] * matrix[5],
      value[0] * matrix[0] + value[1] * matrix[1] + value[2] * matrix[2],
  };
}

template <typename Transform>
void transform_basis(std::array<float, 9> &basis,
                     const std::array<float, 9> &matrix, Transform transform) {
  for (std::size_t row = 0; row < 3; ++row) {
    const std::array<float, 3> source{basis[row * 3], basis[row * 3 + 1],
                                      basis[row * 3 + 2]};
    const auto result = transform(source, matrix);
    for (std::size_t component = 0; component < 3; ++component)
      basis[row * 3 + component] = result[component];
  }
}

[[nodiscard]] std::vector<std::uint32_t>
validated_chain(const std::vector<PictureHierarchyNode> &nodes,
                std::uint32_t start) {
  if (start >= nodes.size())
    throw std::runtime_error("picture hierarchy endpoint is out of range");
  std::vector<bool> visited(nodes.size());
  std::vector<std::uint32_t> chain;
  auto current = start;
  while (current != no_picture_transform_parent) {
    if (current >= nodes.size())
      throw std::runtime_error("picture hierarchy parent is out of range");
    if (visited[current])
      throw std::runtime_error("picture hierarchy contains a cycle");
    visited[current] = true;
    require_finite(nodes[current].matrix,
                   "picture hierarchy matrix is not finite");
    require_finite(nodes[current].position,
                   "picture hierarchy position is not finite");
    chain.push_back(current);
    current = nodes[current].parent;
  }
  return chain;
}

[[nodiscard]] float aligned_axis(float half_extent, float owner_half_extent,
                                 std::uint8_t mask, std::uint8_t axis_mask,
                                 std::uint8_t positive_bit,
                                 std::uint8_t negative_bit) {
  if ((mask & axis_mask) == 0)
    return 0.0F;
  auto result = -half_extent;
  if ((mask & positive_bit) != 0)
    result += owner_half_extent;
  if ((mask & negative_bit) != 0)
    result -= owner_half_extent;
  result = std::floor(result + alignment_rounding_bias);
  if (!std::isfinite(result))
    throw std::runtime_error("picture alignment result is not finite");
  return result;
}

} // namespace

PictureHierarchyTransform produce_picture_hierarchy_transform(
    const std::vector<PictureHierarchyNode> &nodes, std::uint32_t picture_node,
    std::uint32_t owner_node) {
  const auto picture_chain = validated_chain(nodes, picture_node);
  const auto owner_chain = validated_chain(nodes, owner_node);
  if (picture_chain.back() != owner_chain.back())
    throw std::runtime_error("picture and owner hierarchy endpoints differ");

  PictureHierarchyTransform result{
      .basis = {0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F},
      .position = {0.0F, 0.0F, 0.0F},
  };
  for (const auto index : picture_chain) {
    const auto &node = nodes[index];
    transform_basis(result.basis, node.matrix, transform_vector);
    result.position = transform_vector(result.position, node.matrix);
    for (std::size_t axis = 0; axis < result.position.size(); ++axis)
      result.position[axis] += node.position[axis];
  }

  // The terminal/root node is excluded. Reverse traversal then visits the
  // deepest nonterminal ancestor first and the owner last.
  for (auto iterator = owner_chain.rbegin() + 1; iterator != owner_chain.rend();
       ++iterator) {
    const auto &node = nodes[*iterator];
    for (std::size_t axis = 0; axis < result.position.size(); ++axis)
      result.position[axis] -= node.position[axis];
    result.position = inverse_transform_vector(result.position, node.matrix);
    transform_basis(result.basis, node.matrix, inverse_transform_vector);
  }
  require_finite(result.basis, "picture hierarchy result is not finite");
  require_finite(result.position, "picture hierarchy result is not finite");
  return result;
}

std::uint8_t decode_picture_alignment(std::uint32_t value) {
  constexpr std::array<std::uint8_t, 16> mapping{
      0x11, 0x12, 0x14, 0x21, 0x22, 0x24, 0x41, 0x42,
      0x44, 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x00,
  };
  if (value >= mapping.size())
    throw std::runtime_error("picture alignment enum is out of range");
  return mapping[value];
}

std::array<float, 3>
picture_alignment_offset(std::uint32_t alignment,
                         const PictureAlignmentOffsetInput &input) {
  const std::array values{input.picture_half_width, input.picture_half_height,
                          input.owner_half_width, input.owner_half_height};
  require_finite(values, "picture alignment input is not finite");
  if (input.picture_half_width < 0.0F || input.picture_half_height < 0.0F ||
      input.owner_half_width < 0.0F || input.owner_half_height < 0.0F)
    throw std::runtime_error("picture alignment extents must not be negative");

  const auto mask = decode_picture_alignment(alignment);
  return {aligned_axis(input.picture_half_width, input.owner_half_width, mask,
                       0x0f, 0x01, 0x02),
          aligned_axis(input.picture_half_height, input.owner_half_height, mask,
                       0xf0, 0x10, 0x20),
          0.0F};
}

PictureCacheTransform
prepare_picture_cache_transform(const PictureCacheTransformInput &input) {
  require_finite(input.submission_position,
                 "picture submission position is not finite");
  require_finite(input.aligned_local_position,
                 "picture local position is not finite");
  require_finite(input.virtual_window_scale,
                 "picture virtual-window scale is not finite");
  require_finite(input.cached_basis, "picture cached basis is not finite");
  require_finite(input.object_matrix, "picture object matrix is not finite");
  const std::array scalars{
      input.viewport_width,          input.viewport_height,
      input.picture_width,           input.picture_height,
      input.owner_projection_scalar, input.renderer_y_scalar,
  };
  require_finite(scalars, "picture transform scalar is not finite");
  if (input.viewport_width <= 0.0F || input.viewport_height <= 0.0F ||
      input.picture_width < 0.0F || input.picture_height < 0.0F ||
      input.virtual_window_scale[2] == 0.0F ||
      input.owner_projection_scalar == 0.0F)
    throw std::runtime_error("picture transform has an invalid divisor");

  auto translation = input.submission_position;
  translation[0] -= 0.5F;
  translation[1] -= 0.5F;
  translation[0] -= 0.5F * input.viewport_width * input.virtual_window_scale[2];
  translation[1] -=
      0.5F * input.viewport_height * input.virtual_window_scale[3];
  translation[2] += 6.0F;
  for (std::size_t axis = 0; axis < translation.size(); ++axis)
    translation[axis] += input.aligned_local_position[axis];
  if (input.picture_width == 0.0F)
    translation[0] = 0.0F;
  else
    translation[0] /= input.picture_width;
  if (input.picture_height == 0.0F)
    translation[1] = 0.0F;
  else
    translation[1] /= input.picture_height;

  if (!std::isfinite(translation[2]) || translation[2] == 0.0F)
    throw std::runtime_error("picture transform has an invalid depth divisor");
  const auto denominator =
      (input.viewport_width * input.virtual_window_scale[2]) *
      (input.owner_projection_scalar / translation[2]);
  if (!std::isfinite(denominator) || denominator == 0.0F)
    throw std::runtime_error("picture transform denominator is invalid");

  auto basis = input.cached_basis;
  const auto x_scale = input.picture_width / denominator;
  const auto y_scale = -input.picture_height / denominator;
  if (!std::isfinite(x_scale) || !std::isfinite(y_scale))
    throw std::runtime_error("picture transform scale is not finite");
  for (std::size_t component = 6; component < 9; ++component)
    basis[component] *= x_scale;
  for (std::size_t component = 3; component < 6; ++component)
    basis[component] *= input.renderer_y_scalar * y_scale;

  translation = transform_vector(translation, basis);
  std::array<float, 9> composed{};
  for (std::size_t row = 0; row < 3; ++row) {
    const std::array<float, 3> source{basis[row * 3], basis[row * 3 + 1],
                                      basis[row * 3 + 2]};
    const auto transformed = transform_vector(source, input.object_matrix);
    for (std::size_t component = 0; component < 3; ++component)
      composed[row * 3 + component] = transformed[component];
  }
  translation[1] = -translation[1];
  require_finite(translation, "picture cache translation is not finite");
  require_finite(composed, "picture cache basis is not finite");
  return {composed, translation};
}

} // namespace off::graphics
