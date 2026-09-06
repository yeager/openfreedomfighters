#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace off::graphics {

struct PictureAlignmentOffsetInput {
  float center_x{};
  float center_y{};
  float extent_x{};
  float extent_y{};
};

// Decodes the serialized window alignment enum to the runtime axis-bit mask.
// Throws std::runtime_error for values outside the recovered 0..15 domain.
[[nodiscard]] std::uint8_t decode_picture_alignment(std::uint32_t value);

// Uses the same picture's completed center and CLAMPED extents, not parent
// bounds or object translation. Stores an XY offset; returned Z is always zero.
// Finite inputs, nonnegative extents and nearest rounding are native policy.
[[nodiscard]] std::array<float, 3>
picture_alignment_offset(std::uint32_t alignment,
                         const PictureAlignmentOffsetInput &input);

struct PictureCacheTransformInput {
  std::array<float, 3> submission_position{};
  // Original picture adapters supply the stored XY alignment with Z exactly
  // zero. A nonzero Z here is a native extension, not a recovered input.
  std::array<float, 3> aligned_local_position{};
  std::array<float, 4> virtual_window_scale{};
  std::array<float, 9> cached_basis{};
  std::array<float, 9> object_matrix{};
  float viewport_width{};
  float viewport_height{};
  float picture_width{};
  float picture_height{};
  float owner_projection_scalar{};
  // Initialized clean-room replacement for an undefined retail x87 operand.
  // This is an explicit policy input, not a recovered renderer query.
  float external_y_basis_scale{};
};

struct PictureCacheTransform {
  std::array<float, 9> basis{};
  std::array<float, 3> translation{};
};

inline constexpr std::uint32_t no_picture_transform_parent = UINT32_MAX;

struct PictureHierarchyNode {
  std::array<float, 9> matrix{};
  std::array<float, 3> position{};
  std::uint32_t parent{no_picture_transform_parent};
};

struct PictureHierarchyTransform {
  std::array<float, 9> basis{};
  std::array<float, 3> position{};
};

// Produces the object-to-owner values consumed by picture cache preparation.
// The picture chain is accumulated forward through its terminal node. Owner
// adjustment runs deepest-nonterminal-to-owner and deliberately skips the
// terminal node, matching the recovered runtime hierarchy operation.
[[nodiscard]] PictureHierarchyTransform produce_picture_hierarchy_transform(
    const std::vector<PictureHierarchyNode> &nodes, std::uint32_t picture_node,
    std::uint32_t owner_node);

// Reproduces the renderer-neutral ZWINPIC cache preparation after the virtual
// window service has supplied its basis, scale, and scalar values. It does not
// derive or compose a GMS construction hierarchy.
[[nodiscard]] PictureCacheTransform
prepare_picture_cache_transform(const PictureCacheTransformInput &input);

} // namespace off::graphics
