#include "off/graphics/picture_transform.hpp"

#include <array>
#include <cmath>
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
            0, {-1.0F, 1.0F, 1.0F, 1.0F}));
      },
      "reject a negative alignment extent");
  rejects(
      [] {
        static_cast<void>(off::graphics::picture_alignment_offset(
            0, {1.0F, std::numeric_limits<float>::infinity(), 1.0F, 1.0F}));
      },
      "reject a non-finite alignment extent");

  constexpr std::array<float, 9> helper_identity{0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
                                                 0.0F, 1.0F, 0.0F, 0.0F};
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
      .renderer_y_scalar = 2.0F,
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
    auto zero_renderer_y_scale = input;
    zero_renderer_y_scale.renderer_y_scalar = 0.0F;
    const auto transformed =
        off::graphics::prepare_picture_cache_transform(zero_renderer_y_scale);
    check(transformed.basis[3] == 0.0F && transformed.basis[4] == 0.0F &&
              transformed.basis[5] == 0.0F,
          "accept and apply a zero renderer Y multiplier");
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
