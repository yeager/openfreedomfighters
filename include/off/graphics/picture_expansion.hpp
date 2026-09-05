#pragma once

#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/picture_transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::graphics {

inline constexpr std::size_t picture_expansion_limit = 4096;
inline constexpr std::size_t picture_expansion_batch_limit = 2048;

struct ExpandedPictureVertex {
  std::array<float, 3> position{};
  std::array<float, 2> uv{};
  std::uint32_t color{};
};

struct ExpandedPictureBatch {
  // Offset in the supplied descriptor span, not its resource descriptor_index.
  std::size_t first_descriptor{};
  std::vector<ExpandedPictureVertex> vertices;
  // Indices are local to this batch's vertices and restart at zero per batch.
  std::vector<std::uint16_t> indices;
};

// Conditional descriptor-branch geometry only: no projection or compositing.
// Consumes authored centers/spans (not rounded bounds) and the basis unchanged.
// Replacement policy: finite inputs/results, nonnegative spans, at most 4096
// descriptors. Violations throw std::runtime_error. Arithmetic uses double
// intermediates with separately materialized center products and left-to-right
// sums (no multiply-add contraction); each linear center is binary32 before
// adding translation. Translation and corners are then rounded to binary32.
// Values outside binary32's finite range are rejected before conversion. This
// is not a claim of bit-exact retail floating-point evaluation. Only consumed
// authored fields are validated; cached local bounds are ignored.
[[nodiscard]] std::vector<ExpandedPictureBatch>
expand_picture_descriptors(std::span<const data::PictureQuad> descriptors,
                           const PictureCacheTransform &transform);

} // namespace off::graphics
