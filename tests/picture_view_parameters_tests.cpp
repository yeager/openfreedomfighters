#include "off/graphics/picture_view_parameters.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

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
bool close(float a, float b) {
  return std::abs(a - b) <= 0.00001F * std::max(1.0F, std::abs(b));
}
} // namespace

int main() {
  using namespace off::graphics;
  const PictureViewCommonInput common{2, 20, {10, 20, 210, 120}, 400, 100, 3};
  const PictureOrdinaryCameraInput ordinary{std::numbers::pi_v<float> / 2, 2};
  const PictureAlternateCameraInput alternate{0.25F, 0.5F};
  const auto view = prepare_picture_view_parameters(common, ordinary);
  check(view.near_distance == 5 && view.far_distance == 20 &&
            close(view.half_extent_0, 5) && close(view.half_extent_1, 15),
        "ordinary radians, asymmetric aspect, explicit E and near clamp");
  const auto alt = prepare_picture_view_parameters(common, alternate);
  check(alt.near_distance == 5 && alt.far_distance == 40 &&
            alt.half_extent_0 == 2 && alt.half_extent_1 == 6,
        "alternate scalar equations and doubled selected far");
  check(project_picture_position(alt.projection, {1, 2, 8})[3] == 8,
        "alternate camera branch still produces perspective clip W=Z");
  auto signed_common = common;
  signed_common.rectangle.right = -190;
  const auto signed_view =
      prepare_picture_view_parameters(signed_common, ordinary);
  check(close(signed_view.half_extent_0, 5) &&
            close(signed_view.half_extent_1, -15),
        "signed rectangle dimensions retain sign");
  auto no_clamp = common;
  no_clamp.raw_near = 10;
  check(prepare_picture_view_parameters(no_clamp, ordinary).near_distance == 10,
        "near above lower bound is unchanged");
  auto reflected = alternate;
  reflected.parameter_0 = -0.25F;
  reflected.parameter_1 = -0.5F;
  const auto reflected_view =
      prepare_picture_view_parameters(common, reflected);
  check(reflected_view.half_extent_0 == -2 &&
            reflected_view.half_extent_1 == -6,
        "signed alternate scalars are retained");
  for (unsigned dimensions = 1; dimensions <= 3; ++dimensions) {
    auto zero = common;
    if (dimensions & 1)
      zero.rectangle.right = zero.rectangle.left;
    if (dimensions & 2)
      zero.rectangle.bottom = zero.rectangle.top;
    const auto result = prepare_picture_view_parameters(zero, ordinary);
    const float multiplier = dimensions == 1   ? 600.0F
                             : dimensions == 2 ? 0.03F
                                               : 6.0F;
    check(close(result.half_extent_1, result.half_extent_0 * multiplier),
          "projection-only zero-dimension replacement");
    const auto viewport =
        prepare_picture_viewport_request(zero.rectangle, {0, 0, 1, 1});
    check((!(dimensions & 1) || viewport[2] == 0) &&
              (!(dimensions & 2) || viewport[3] == 0),
          "viewport producer retains original zero dimensions");
  }
  check(prepare_picture_viewport_request(common.rectangle,
                                         {-0.5F, 2, 1.5F, -0.5F}) ==
            std::array<float, 4>{10, 220, 200, -50},
        "viewport bounds are one-sided, not full clamps");
  const auto raw = prepare_picture_viewport_request(common.rectangle,
                                                    {0.25F, 0.5F, 0.5F, 0.25F});
  check(raw == std::array<float, 4>{60, 70, 100, 25},
        "c/d are viewport extents, not opposite endpoints");
  const auto viewport = convert_picture_viewport_request(raw);
  check(map_picture_clip_to_viewport({0, 0, 0.5F, 1}, viewport) ==
            std::array<float, 3>{110, 82.5F, 0.5F},
        "raw producer integrates with explicit conversion and screen mapping");
  for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
    for (auto member : {&PictureViewCommonInput::raw_near,
                        &PictureViewCommonInput::selected_far,
                        &PictureViewCommonInput::renderer_dimension_0,
                        &PictureViewCommonInput::renderer_dimension_1,
                        &PictureViewCommonInput::virtual_aspect}) {
      auto bad = common;
      bad.*member = invalid;
      rejects(
          [&] {
            static_cast<void>(prepare_picture_view_parameters(bad, ordinary));
          },
          "reject every nonfinite common scalar");
    }
    for (auto member :
         {&PicturePassRectangle::left, &PicturePassRectangle::top,
          &PicturePassRectangle::right, &PicturePassRectangle::bottom}) {
      auto bad = common;
      bad.rectangle.*member = invalid;
      rejects(
          [&] {
            static_cast<void>(prepare_picture_view_parameters(bad, alternate));
          },
          "reject every nonfinite rectangle coordinate");
      rejects(
          [&] {
            static_cast<void>(
                prepare_picture_viewport_request(bad.rectangle, {0, 0, 1, 1}));
          },
          "raw viewport rejects nonfinite rectangle");
    }
    for (std::size_t i = 0; i < 4; ++i) {
      std::array<float, 4> bad{0, 0, 1, 1};
      bad[i] = invalid;
      rejects(
          [&] {
            static_cast<void>(
                prepare_picture_viewport_request(common.rectangle, bad));
          },
          "reject every nonfinite viewport parameter");
    }
    for (auto camera : {PictureOrdinaryCameraInput{invalid, 1}, {1, invalid}})
      rejects(
          [&] {
            static_cast<void>(prepare_picture_view_parameters(common, camera));
          },
          "reject nonfinite ordinary camera inputs");
    for (auto camera : {PictureAlternateCameraInput{invalid, 1}, {1, invalid}})
      rejects(
          [&] {
            static_cast<void>(prepare_picture_view_parameters(common, camera));
          },
          "reject nonfinite alternate camera inputs");
  }
  for (auto member : {&PictureViewCommonInput::renderer_dimension_0,
                      &PictureViewCommonInput::renderer_dimension_1,
                      &PictureViewCommonInput::virtual_aspect}) {
    auto bad = common;
    bad.*member = 0;
    rejects(
        [&] {
          static_cast<void>(prepare_picture_view_parameters(bad, ordinary));
        },
        "zero ratio divisor or final half-extent is rejected");
  }
  for (auto camera : {PictureOrdinaryCameraInput{0, 1}, {1, 0}})
    rejects(
        [&] {
          static_cast<void>(prepare_picture_view_parameters(common, camera));
        },
        "zero ordinary half-extent rejected by projection safety policy");
  for (auto camera : {PictureAlternateCameraInput{0, 1}, {1, 0}})
    rejects(
        [&] {
          static_cast<void>(prepare_picture_view_parameters(common, camera));
        },
        "zero alternate divisor rejected");
  auto bad_far = common;
  bad_far.selected_far = 2;
  rejects(
      [&] {
        static_cast<void>(prepare_picture_view_parameters(bad_far, alternate));
      },
      "doubled far must still exceed clamped near");
  constexpr float maximum = std::numeric_limits<float>::max();
  auto huge_far = common;
  huge_far.selected_far = maximum;
  rejects(
      [&] {
        static_cast<void>(prepare_picture_view_parameters(huge_far, alternate));
      },
      "doubled far overflow rejected before conversion");
  rejects(
      [&] {
        static_cast<void>(prepare_picture_view_parameters(
            common,
            PictureOrdinaryCameraInput{ordinary.angle_radians, maximum}));
      },
      "final half-extent overflow rejected before conversion");
  rejects(
      [&] {
        static_cast<void>(prepare_picture_view_parameters(
            common, PictureAlternateCameraInput{
                        std::numeric_limits<float>::denorm_min(), 1}));
      },
      "alternate reciprocal overflow rejected");
  rejects(
      [&] {
        static_cast<void>(prepare_picture_viewport_request(
            {-maximum, 0, maximum, 1}, {0, 0, 1, 1}));
      },
      "raw viewport extent overflow rejected");
  rejects(
      [&] {
        static_cast<void>(prepare_picture_viewport_request(common.rectangle,
                                                           {maximum, 0, 1, 1}));
      },
      "raw viewport offset overflow rejected");
  return failures == 0 ? 0 : 1;
}
