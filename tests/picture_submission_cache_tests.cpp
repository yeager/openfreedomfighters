#include "off/graphics/picture_submission_cache.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
template <class Function> void rejects(Function function, const char *message) {
  try {
    function();
    check(false, message);
  } catch (const std::runtime_error &) {
  }
}
off::graphics::PictureCacheTransformInput input() {
  constexpr std::array<float, 9> basis{0, 0, 1, 0, 1, 0, 1, 0, 0};
  return {.submission_position = {0, 0, 0},
          .aligned_local_position = {50, 40, 0},
          .virtual_window_scale = {7, 9, 1, 1},
          .cached_basis = basis,
          .object_matrix = basis,
          .viewport_width = 100,
          .viewport_height = 80,
          .picture_width = 20,
          .picture_height = 10,
          .owner_projection_scalar = 2,
          .external_y_basis_scale = 2};
}
} // namespace

int main() {
  using namespace off::graphics;
  static_assert(!std::is_copy_constructible_v<PictureSubmissionCache>);
  static_assert(!std::is_move_constructible_v<PictureSubmissionCache>);
  static_assert(!std::is_copy_assignable_v<PictureSubmissionCache>);
  static_assert(!std::is_move_assignable_v<PictureSubmissionCache>);
  PictureSubmissionCache cache;
  check(cache.dirty() && !cache.cached_state(),
        "new cache has no invented usable transform");
  std::array<off::data::BoundPictureDrawGroup, 3> groups;
  groups[0].quads.resize(2);
  groups[2].quads.resize(1); // Middle group is intentionally empty.
  groups[0].quads[0].descriptor_index = 8;
  groups[0].quads[1].descriptor_index = 3;
  const auto table = std::span<const off::data::BoundPictureDrawGroup>(groups);
  auto request = input();
  std::vector<std::size_t> visited;
  std::uint32_t expected_control = 17;
  const auto visitor = [&](std::size_t index, const auto &group,
                           const auto &transform, std::uint32_t control) {
    visited.push_back(index);
    check(&group == &groups[index] && control == expected_control,
          "visitor receives current paired group and submission control");
    check(transform.basis == cache.cached_state()->transform.basis,
          "visitor sees committed cached transform");
  };
  cache.submit(table, request, expected_control, visitor);
  check(!cache.dirty() && cache.cached_state() &&
            visited == std::vector<std::size_t>{0, 1, 2},
        "first prepare visits all groups including empty in authored order");
  check(groups[0].quads[0].descriptor_index == 8 &&
            groups[0].quads[1].descriptor_index == 3,
        "descriptor order untouched");
  const auto first = *cache.cached_state();
  groups[0].texture.prm_offset = 99;
  groups[0].texture.authored_texture_resource_record[0] = std::byte{0x42};
  expected_control = 123;
  visited.clear();
  request.viewport_width = std::numeric_limits<float>::quiet_NaN();
  cache.submit(
      table, request, expected_control,
      [&](auto index, const auto &group, const auto &transform, auto control) {
        visitor(index, group, transform, control);
        if (index == 0)
          check(group.texture.prm_offset == 99 &&
                    group.texture.authored_texture_resource_record[0] ==
                        std::byte{0x42},
                "clean reuse uses current resource binding");
      });
  check(
      visited.size() == 3 && cache.cached_state()->transform.translation ==
                                 first.transform.translation,
      "clean cache ignores changed nonposition dependencies but still visits");
  request = input();
  for (std::size_t axis = 0; axis < 3; ++axis) {
    request.submission_position[axis] += 1;
    const auto expected = prepare_picture_cache_transform(request);
    cache.submit(table, request, expected_control, visitor);
    check(cache.cached_state()->position == request.submission_position &&
              cache.cached_state()->transform.translation ==
                  expected.translation,
          "any unequal position axis recomputes exactly");
  }
  PictureSubmissionCache zero_cache;
  zero_cache.submit(table, input(), 0,
                    [](auto, const auto &, const auto &, auto) {});
  auto signed_zero = input();
  signed_zero.submission_position = {-0.0F, -0.0F, -0.0F};
  signed_zero.viewport_width = 0; // Would fail if preparation were invoked.
  zero_cache.submit(table, signed_zero, 0,
                    [](auto, const auto &, const auto &, auto) {});
  check(!zero_cache.dirty(), "signed zero position compares numerically equal");
  PictureSubmissionCache absent;
  auto invalid = input();
  invalid.submission_position[0] = std::numeric_limits<float>::quiet_NaN();
  absent.submit(std::nullopt, invalid, 0, {});
  check(absent.dirty() && !absent.cached_state(),
        "absent table skips all validation and state changes");
  absent.submit(std::span<const off::data::BoundPictureDrawGroup>{}, input(), 0,
                {});
  check(!absent.dirty() && absent.cached_state(),
        "present empty table prepares without requiring visitor");
  absent.invalidate();
  const auto previous_empty = *absent.cached_state();
  absent.submit(std::nullopt, invalid, 0, {});
  check(absent.dirty() &&
            absent.cached_state()->position == previous_empty.position,
        "absent table preserves populated dirty cache");
  const auto before_failure = *cache.cached_state();
  for (float hostile : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      auto bad = request;
      bad.submission_position[axis] = hostile;
      rejects([&] { cache.submit(table, bad, 0, visitor); },
              "present table rejects every nonfinite position axis");
      check(!cache.dirty() &&
                cache.cached_state()->position == before_failure.position,
            "invalid position leaves state untouched");
    }
  }
  auto changed = request;
  changed.submission_position[0] += 1;
  rejects([&] { cache.submit(table, changed, 0, {}); },
          "missing required visitor rejected before mutation");
  check(!cache.dirty() &&
            cache.cached_state()->position == before_failure.position,
        "empty visitor failure retains clean state");
  changed.viewport_width = 0;
  rejects([&] { cache.submit(table, changed, 0, visitor); },
          "prepare failure propagates");
  check(cache.dirty() &&
            cache.cached_state()->position == before_failure.position &&
            cache.cached_state()->transform.translation ==
                before_failure.transform.translation,
        "prepare failure retains previous snapshot but marks dirty");
  changed.viewport_width = 200;
  cache.submit(table, changed, expected_control, visitor);
  const auto changed_transform = cache.cached_state()->transform;
  changed.viewport_width = 100;
  cache.invalidate();
  cache.submit(table, changed, expected_control, visitor);
  check(!cache.dirty() && cache.cached_state()->transform.translation !=
                              changed_transform.translation,
        "explicit invalidation refreshes changed viewport dependency");
  visited.clear();
  changed.submission_position[2] += 1;
  rejects(
      [&] {
        cache.submit(table, changed, 0,
                     [&](auto index, const auto &, const auto &, auto) {
                       visited.push_back(index);
                       if (index == 1)
                         throw std::runtime_error("visitor failure");
                     });
      },
      "visitor failure propagates");
  check(visited == std::vector<std::size_t>{0, 1} && !cache.dirty() &&
            cache.cached_state()->position == changed.submission_position,
        "visitor failure preserves committed transform and observed prefix");
  cache.submit(table, changed, 0,
               [&](auto index, const auto &, const auto &, auto) {
                 if (index == 0)
                   cache.invalidate();
               });
  check(cache.dirty(), "callback invalidation retained for next submission");
  cache.submit(table, changed, 0,
               [&](auto index, const auto &, const auto &, auto) {
                 if (index == 0)
                   rejects([&] { cache.submit(table, changed, 0, visitor); },
                           "reentrant submission on same cache rejected");
               });
  check(
      !cache.dirty(),
      "outer successful submission completes after caught reentrant rejection");
  cache.submit(table, changed, expected_control, visitor);
  return failures == 0 ? 0 : 1;
}
