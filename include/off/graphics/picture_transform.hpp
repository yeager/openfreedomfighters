#pragma once

#include <array>
#include <cstdint>

namespace off::graphics {

struct PictureAlignmentOffsetInput {
  float picture_half_width{};
  float picture_half_height{};
  float owner_half_width{};
  float owner_half_height{};
};

// Decodes the serialized window alignment enum to the runtime axis-bit mask.
// Throws std::runtime_error for values outside the recovered 0..15 domain.
[[nodiscard]] std::uint8_t decode_picture_alignment(std::uint32_t value);

// Reproduces the window alignment arithmetic. The returned Z component is zero.
[[nodiscard]] std::array<float, 3>
picture_alignment_offset(std::uint32_t alignment,
                         const PictureAlignmentOffsetInput &input);

struct PictureCacheTransformInput {
  std::array<float, 3> submission_position{};
  std::array<float, 3> aligned_local_position{};
  std::array<float, 4> virtual_window_scale{};
  std::array<float, 9> cached_basis{};
  std::array<float, 9> object_matrix{};
  float viewport_width{};
  float viewport_height{};
  float picture_width{};
  float picture_height{};
  float owner_projection_scalar{};
  float renderer_y_scalar{};
};

struct PictureCacheTransform {
  std::array<float, 9> basis{};
  std::array<float, 3> translation{};
};

// Reproduces the renderer-neutral ZWINPIC cache preparation after the virtual
// window service has supplied its basis, scale, and scalar values. It does not
// derive or compose a GMS construction hierarchy.
[[nodiscard]] PictureCacheTransform
prepare_picture_cache_transform(const PictureCacheTransformInput &input);

} // namespace off::graphics
