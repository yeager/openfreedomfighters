#include "off/graphics/picture_transform.hpp"

#include <array>
#include <cmath>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Function>
void rejects(Function function, const char *message) {
  try {
    function();
    check(false, message);
  } catch (const std::runtime_error &) {
  }
}

bool close(float left, float right) {
  return std::abs(left - right) <= 0.00001F;
}

} // namespace

int main() {
  constexpr std::array<std::uint8_t, 16> masks{
      0x11, 0x12, 0x14, 0x21, 0x22, 0x24, 0x41, 0x42,
      0x44, 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x00,
  };
  constexpr std::array<std::array<float, 2>, 16> offsets{{
      {90, 45},
      {-110, 45},
      {-10, 45},
      {90, -55},
      {-110, -55},
      {-10, -55},
      {90, -5},
      {-110, -5},
      {-10, -5},
      {90, 0},
      {-110, 0},
      {-10, 0},
      {0, 45},
      {0, -55},
      {0, -5},
      {0, 0},
  }};
  for (std::uint32_t value = 0; value < masks.size(); ++value) {
    check(off::graphics::decode_picture_alignment(value) == masks[value],
          "decode every recovered alignment enum");
    const auto offset = off::graphics::picture_alignment_offset(
        value, {10.0F, 5.0F, 100.0F, 50.0F});
    check(offset[0] == offsets[value][0] && offset[1] == offsets[value][1] &&
              offset[2] == 0.0F,
          "apply recovered alignment arithmetic");
  }
  rejects(
      [] { static_cast<void>(off::graphics::decode_picture_alignment(16)); },
      "reject an unknown alignment enum");
  rejects(
      [] {
        static_cast<void>(off::graphics::picture_alignment_offset(
            0, {1.0F, 1.0F, -1.0F, 1.0F}));
      },
      "reject a negative alignment extent");
  rejects(
      [] {
        static_cast<void>(off::graphics::picture_alignment_offset(
            0, {1.0F, std::numeric_limits<float>::infinity(), 1.0F, 1.0F}));
      },
      "reject a non-finite alignment extent");
  check(off::graphics::picture_alignment_offset(0, {-3, -4, 3, 4}) ==
            std::array<float, 3>{6, 8, 0},
        "negative picture centers are valid and are not half extents");
  check(off::graphics::picture_alignment_offset(8, {-3, 4, 99, 99}) ==
            std::array<float, 3>{3, -4, 0},
        "center-only alignment ignores the same picture extents");
  check(off::graphics::picture_alignment_offset(0, {0x1p-12F, 0, 0x1p-13F, 0x1p-13F}) ==
            std::array<float, 3>{0, 0, 0},
        "alignment consumes clamped extents before the rounding bias");
  check(off::graphics::picture_alignment_offset(8, {0.25F, -0.25F, 1, 1}) ==
            std::array<float, 3>{-1, 0, 0},
        "alignment floors fractional signed centers rather than truncating");
  const int rounding = std::fegetround();
  if (std::fesetround(FE_UPWARD) == 0) {
    rejects([] { (void)off::graphics::picture_alignment_offset(0, {1, 2, 3, 4}); },
            "reject unsupported alignment rounding mode");
    check(std::fesetround(rounding) == 0, "restore alignment rounding mode");
  }

  constexpr std::array<float, 9> helper_identity{0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
                                                 0.0F, 1.0F, 0.0F, 0.0F};
  {
    const std::vector<off::graphics::PictureHierarchyNode> nodes{
        {.matrix = helper_identity, .position = {100.0F, 100.0F, 100.0F}},
        {.matrix = helper_identity,
         .position = {10.0F, 20.0F, 30.0F},
         .parent = 0},
        {.matrix = helper_identity,
         .position = {5.0F, 6.0F, 7.0F},
         .parent = 0},
    };
    const auto hierarchy =
        off::graphics::produce_picture_hierarchy_transform(nodes, 1, 2);
    check(hierarchy.basis == helper_identity,
          "preserve the recovered hierarchy basis seed");
    check(hierarchy.position == std::array<float, 3>{105.0F, 114.0F, 123.0F},
          "accumulate picture chain and skip terminal owner adjustment");
  }
  {
    auto owner_scale = helper_identity;
    owner_scale[6] = 2.0F;
    owner_scale[4] = 3.0F;
    owner_scale[2] = 4.0F;
    const std::vector<off::graphics::PictureHierarchyNode> nodes{
        {.matrix = helper_identity},
        {.matrix = helper_identity,
         .position = {1.0F, 2.0F, 3.0F},
         .parent = 0},
        {.matrix = owner_scale, .position = {1.0F, 1.0F, 1.0F}, .parent = 0},
    };
    const auto hierarchy =
        off::graphics::produce_picture_hierarchy_transform(nodes, 1, 2);
    check(hierarchy.position == std::array<float, 3>{0.0F, 3.0F, 8.0F},
          "apply owner transpose-form operation after subtracting position");
    check(close(hierarchy.basis[2], 4.0F) && close(hierarchy.basis[4], 3.0F) &&
              close(hierarchy.basis[6], 2.0F),
          "apply the owner operation independently to every basis vector");
  }
  rejects(
      [&] {
        const std::vector<off::graphics::PictureHierarchyNode> nodes{
            {.matrix = helper_identity, .parent = 0}};
        static_cast<void>(
            off::graphics::produce_picture_hierarchy_transform(nodes, 0, 0));
      },
      "reject a hierarchy cycle");
  rejects(
      [&] {
        const std::vector<off::graphics::PictureHierarchyNode> nodes{
            {.matrix = helper_identity, .parent = 2}};
        static_cast<void>(
            off::graphics::produce_picture_hierarchy_transform(nodes, 0, 0));
      },
      "reject an out-of-range hierarchy parent");
  rejects(
      [&] {
        const std::vector<off::graphics::PictureHierarchyNode> nodes{
            {.matrix = helper_identity}, {.matrix = helper_identity}};
        static_cast<void>(
            off::graphics::produce_picture_hierarchy_transform(nodes, 0, 1));
      },
      "reject mismatched hierarchy endpoints");
  rejects(
      [&] {
        auto bad_matrix = helper_identity;
        bad_matrix[0] = std::numeric_limits<float>::quiet_NaN();
        const std::vector<off::graphics::PictureHierarchyNode> nodes{
            {.matrix = bad_matrix}};
        static_cast<void>(
            off::graphics::produce_picture_hierarchy_transform(nodes, 0, 0));
      },
      "reject non-finite hierarchy inputs");
  rejects(
      [&] {
        const std::vector<off::graphics::PictureHierarchyNode> nodes{
            {.matrix = helper_identity}};
        static_cast<void>(
            off::graphics::produce_picture_hierarchy_transform(nodes, 1, 0));
      },
      "reject an out-of-range hierarchy endpoint");
  off::graphics::PictureCacheTransformInput input{
      .submission_position = {0.5F, 1.5F, 4.0F},
      .aligned_local_position = {50.0F, 40.0F, 0.0F},
      .virtual_window_scale = {7.0F, 9.0F, 1.0F, 1.0F},
      .cached_basis = helper_identity,
      .object_matrix = helper_identity,
      .viewport_width = 100.0F,
      .viewport_height = 80.0F,
      .picture_width = 20.0F,
      .picture_height = 10.0F,
      .owner_projection_scalar = 2.0F,
      .external_y_basis_scale = 2.0F,
  };
  const auto result = off::graphics::prepare_picture_cache_transform(input);
  const std::array<float, 9> expected_basis{0.0F, 0.0F, 1.0F, 0.0F, -1.0F,
                                            0.0F, 1.0F, 0.0F, 0.0F};
  check(result.basis == expected_basis, "compose the exact cached basis order");
  check(close(result.translation[0], 0.0F) &&
            close(result.translation[1], 0.1F) &&
            close(result.translation[2], 10.0F),
        "apply both recovered Y sign operations and retain depth");

  auto translated = input;
  translated.aligned_local_position[2] = 3.0F;
  const auto with_local_z =
      off::graphics::prepare_picture_cache_transform(translated);
  check(close(with_local_z.translation[2], 13.0F),
        "preserve local Z through cache preparation");

  {
    auto zero_width = input;
    zero_width.picture_width = 0.0F;
    const auto collapsed =
        off::graphics::prepare_picture_cache_transform(zero_width);
    check(collapsed.basis[6] == 0.0F && collapsed.basis[7] == 0.0F &&
              collapsed.basis[8] == 0.0F &&
              close(collapsed.translation[0], 0.0F),
          "collapse normalized X and X basis scale for zero width");
  }
  {
    auto zero_height = input;
    zero_height.picture_height = 0.0F;
    const auto collapsed =
        off::graphics::prepare_picture_cache_transform(zero_height);
    check(collapsed.basis[3] == 0.0F && collapsed.basis[4] == 0.0F &&
              collapsed.basis[5] == 0.0F &&
              close(collapsed.translation[1], 0.0F),
          "collapse normalized Y and Y basis scale for zero height");
  }
  {
    auto zero_window_y_scale = input;
    zero_window_y_scale.virtual_window_scale[3] = 0.0F;
    const auto transformed =
        off::graphics::prepare_picture_cache_transform(zero_window_y_scale);
    check(std::isfinite(transformed.translation[1]),
          "accept zero virtual-window Y scale as a multiplier");
  }
  {
    auto zero_external_y_scale = input;
    zero_external_y_scale.external_y_basis_scale = 0.0F;
    const auto transformed =
        off::graphics::prepare_picture_cache_transform(zero_external_y_scale);
    check(transformed.basis[3] == 0.0F && transformed.basis[4] == 0.0F &&
              transformed.basis[5] == 0.0F,
          "accept and apply a zero external Y basis multiplier");
  }
  for (const auto invalid : {-1.0F}) {
    auto bad = input;
    bad.picture_width = invalid;
    rejects(
        [&] {
          static_cast<void>(
              off::graphics::prepare_picture_cache_transform(bad));
        },
        "reject an invalid picture divisor");
  }
  {
    auto bad = input;
    bad.submission_position[2] = -6.0F;
    rejects(
        [&] {
          static_cast<void>(
              off::graphics::prepare_picture_cache_transform(bad));
        },
        "reject a zero depth divisor");
  }
  {
    auto bad = input;
    bad.object_matrix[8] = std::numeric_limits<float>::quiet_NaN();
    rejects(
        [&] {
          static_cast<void>(
              off::graphics::prepare_picture_cache_transform(bad));
        },
        "reject a non-finite object matrix");
  }
  {
    auto bad = input;
    bad.owner_projection_scalar = 0.0F;
    rejects(
        [&] {
          static_cast<void>(
              off::graphics::prepare_picture_cache_transform(bad));
        },
        "reject a zero projection scalar");
  }

  return failures == 0 ? 0 : 1;
}
