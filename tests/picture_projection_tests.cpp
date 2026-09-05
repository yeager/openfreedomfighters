#include "off/graphics/picture_expansion.hpp"
#include "off/graphics/picture_projection.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
int failures{};
void check(bool condition, const char *message) {
  if (!condition) {
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
bool close(float a, float b) { return std::abs(a - b) < 0.00001F; }
} // namespace

int main() {
  using namespace off::graphics;
  static_assert(!std::is_default_constructible_v<PictureProjection>);
  const auto projection = prepare_picture_projection(5, 10, -10, 2.5F);
  const std::array<float, 16> expected{-0.5F, 0, 0, 0, 0, 2, 0,   0,
                                       0,     0, 2, 1, 0, 0, -10, 0};
  check(projection.matrix() == expected,
        "exact ordinary row-order matrix with asymmetric signed half-extents");
  const auto near_clip = project_picture_position(projection, {4, 3, 5});
  const auto far_clip = project_picture_position(projection, {4, 3, 10});
  check(near_clip == std::array<float, 4>{-2, 6, 0, 5} &&
            far_clip == std::array<float, 4>{-2, 6, 10, 10},
        "clip W equals emitted Z with ordinary near/far depth coefficients");
  const auto near_screen =
      map_picture_clip_to_viewport(near_clip, {0, 0, 100, 100});
  const auto far_screen =
      map_picture_clip_to_viewport(far_clip, {0, 0, 100, 100});
  check(near_screen[0] == 30 && far_screen[0] == 40 && near_screen[2] == 0 &&
            far_screen[2] == 1,
        "perspective position changes with Z and maps near/far to depth "
        "zero/one");
  check(map_picture_clip_to_viewport({2, 1, 1, 2}, {10, 20, 100, 80}) ==
            std::array<float, 3>{110, 40, 0.5F},
        "nonzero viewport origin and Y flip");
  check(map_picture_clip_to_viewport({2, 1, 1, -2}, {10, 20, 100, 80}) ==
            std::array<float, 3>{10, 80, -0.5F},
        "negative W remains algebraic without visibility or depth-clamp claim");
  const auto vertical_reflection = prepare_picture_projection(5, 10, 10, -2.5F);
  check(vertical_reflection.matrix()[0] == 0.5F &&
            vertical_reflection.matrix()[5] == -2,
        "either signed nonzero half-extent is accepted");
  off::data::PictureQuad quad{.local_z = 10,
                              .local_center_x = 2,
                              .local_center_y = 4,
                              .horizontal_edge_span = 2,
                              .vertical_edge_span = 2};
  const auto expanded = expand_picture_descriptors(
      std::span(&quad, 1), {{0, 0, 1, 0, 3, 0, 2, 0, 0}, {0, 0, 0}});
  const auto clip =
      project_picture_position(prepare_picture_projection(5, 10, 5, 5),
                               expanded[0].vertices[0].position);
  check(clip == std::array<float, 4>{2, 9, 10, 10},
        "expanded picture coordinates receive projection without repeated "
        "picture basis");
  const auto screen = map_picture_clip_to_viewport(clip, {0, 0, 100, 100});
  check(
      close(screen[0], 60) && close(screen[1], 5) && screen[2] == 1,
      "expanded picture vertex integrates into conditional viewport equation");
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    for (std::size_t i = 0; i < 4; ++i) {
      std::array<float, 4> parameters{5, 10, 5, 5};
      parameters[i] = invalid;
      rejects(
          [&] {
            static_cast<void>(prepare_picture_projection(
                parameters[0], parameters[1], parameters[2], parameters[3]));
          },
          "reject every nonfinite projection parameter");
      std::array<float, 4> bad_clip{1, 1, 1, 1};
      bad_clip[i] = invalid;
      rejects(
          [&] {
            static_cast<void>(
                map_picture_clip_to_viewport(bad_clip, {0, 0, 1, 1}));
          },
          "reject every nonfinite clip component");
      if (i < 3) {
        std::array<float, 3> position{1, 1, 1};
        position[i] = invalid;
        rejects(
            [&] {
              static_cast<void>(project_picture_position(projection, position));
            },
            "reject every nonfinite input position");
      }
    }
  }
  for (const auto parameters : {std::array<float, 4>{4, 10, 5, 5},
                                {5, 5, 5, 5},
                                {5, 4, 5, 5},
                                {5, 10, 0, 5},
                                {5, 10, 5, -0.0F}})
    rejects(
        [&] {
          static_cast<void>(prepare_picture_projection(
              parameters[0], parameters[1], parameters[2], parameters[3]));
        },
        "reject invalid resolved frustum without silently clamping");
  constexpr auto uint_max = std::numeric_limits<std::uint32_t>::max();
  for (const PictureViewport viewport : {PictureViewport{0, 0, 0, 1},
                                         {0, 0, 1, 0},
                                         {uint_max, 0, 1, 1},
                                         {0, uint_max, 1, 1}})
    rejects(
        [&] {
          static_cast<void>(
              map_picture_clip_to_viewport({0, 0, 0, 1}, viewport));
        },
        "reject zero viewport extent and widened edge overflow");
  check(std::isfinite(map_picture_clip_to_viewport(
            {0, 0, 0, 1}, {uint_max - 1, uint_max - 1, 1, 1})[0]),
        "accept viewport edge exactly at uint32 maximum");
  for (float zero : {0.0F, -0.0F})
    rejects(
        [&] {
          static_cast<void>(
              map_picture_clip_to_viewport({1, 1, 1, zero}, {0, 0, 1, 1}));
        },
        "reject either signed zero W");
  constexpr float max = std::numeric_limits<float>::max();
  constexpr float tiny = std::numeric_limits<float>::denorm_min();
  rejects(
      [&] { static_cast<void>(prepare_picture_projection(5, 10, tiny, 5)); },
      "reject horizontal coefficient overflow before binary32 conversion");
  rejects(
      [&] { static_cast<void>(prepare_picture_projection(5, 10, 5, tiny)); },
      "reject vertical coefficient overflow before binary32 conversion");
  rejects(
      [&] {
        static_cast<void>(prepare_picture_projection(max * 0.75F, max, 5, 5));
      },
      "reject coefficient/depth overflow for huge near/far");
  for (const auto position : {std::array<float, 3>{0, max, 5}, {0, 0, max}})
    rejects(
        [&] {
          static_cast<void>(project_picture_position(projection, position));
        },
        "reject clip overflow before conversion");
  for (const auto bad_clip : {std::array<float, 4>{max, 0, 0, tiny},
                              {0, max, 0, tiny},
                              {0, 0, max, tiny}})
    rejects(
        [&] {
          static_cast<void>(
              map_picture_clip_to_viewport(bad_clip, {0, 0, 100, 100}));
        },
        "reject each screen coordinate overflow before conversion");
  return failures == 0 ? 0 : 1;
}
