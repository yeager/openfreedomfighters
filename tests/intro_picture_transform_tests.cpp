#include "off/graphics/fresh_intro_camera.hpp"
#include "off/graphics/picture_expansion.hpp"
#include "off/graphics/picture_submission_cache.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
off::data::GmsIntroCameraSource source() {
  off::data::GmsIntroCameraSource result{};
  result.near_distance = 2;
  result.far_distance = 17;
  result.angle_degrees = 60;
  result.viewport = {0.25F, 0.5F, 0.75F, 0.375F};
  return result;
}
constexpr std::array<float, 9> identity{0, 0, 1, 0, 1, 0, 1, 0, 0};
}

int main() {
  const int saved_rounding = std::fegetround();
  if (std::fesetround(FE_TONEAREST) != 0) return 1;
  auto parameters = convert_intro_camera_mode_zero(source());
  // Independent arithmetic witness: preserving far multiplication/division
  // yields 0x3f6a4d6b, while cancelling far yields 0x3f6a4d6c.
  parameters.angle_radians = 1.0F;
  CameraEnabledState flags(0xabcdefeeU);
  const auto services = prepare_picture_camera_services(flags, parameters, {-10, 17, 90, 67});
  check(flags.flags() == 0xabcdefecU && flags.enabled(),
        "preparation clears only canonical 0x2 and preserves enabled and unrelated bits");
  check(services.viewport == parameters.viewport && services.pass_dimensions == std::array<float, 2>{100, 50},
        "service values use converted viewport and signed stored rectangle extents");
  check(std::bit_cast<std::uint32_t>(services.projection_scalar) == 0x3f6a4d6bU,
        "camera scalar preserves far-dependent binary32 stage order");
  const auto shifted = prepare_picture_camera_services(flags, parameters, {20, -70, 120, -20});
  check(shifted.pass_dimensions == services.pass_dimensions && shifted.projection_scalar == services.projection_scalar,
        "rectangle origin does not become picture dimensions");
  parameters.near_distance = std::numeric_limits<float>::quiet_NaN();
  check(prepare_picture_camera_services(flags, parameters, {0, 0, 100, 50}).projection_scalar == services.projection_scalar,
        "near is not an input to the bounded picture scalar projection");

  for (const auto rectangle : std::array<PictureVisitorRectangle, 5>{{
      {1, 0, 1, 10}, {10, 0, 1, 10}, {0, 5, 10, 5},
      {std::numeric_limits<std::int32_t>::min(), 0, std::numeric_limits<std::int32_t>::max(), 1},
      {0, std::numeric_limits<std::int32_t>::min(), 1, 0}}}) {
    CameraEnabledState state(0x22U);
    rejects([&] { (void)prepare_picture_camera_services(state, parameters, rectangle); }, "invalid signed rectangle rejects");
    check(state.flags() == 0x22U, "rectangle prevalidation precedes canonical writes");
  }
  {
    CameraEnabledState state(2);
    const auto large = prepare_picture_camera_services(state, parameters,
        {0, 0, 16777217, std::numeric_limits<std::int32_t>::max()});
    check(large.pass_dimensions == std::array<float, 2>{16777216.0F, 2147483648.0F},
          "signed32 extent converts to binary32 after subtraction, including rounding");
  }
  const auto invalid_camera = [&](auto mutate) {
    auto p = parameters; mutate(p);
    CameraEnabledState state(0x22U);
    rejects([&] { (void)prepare_picture_camera_services(state, p, {0, 0, 100, 50}); }, "invalid camera scalar input rejects");
    check(state.flags() == 0x22U, "camera arithmetic failure precedes canonical clear");
  };
  invalid_camera([](auto& p) { p.angle_radians = 0; });
  invalid_camera([](auto& p) { p.angle_radians = std::numeric_limits<float>::infinity(); });
  invalid_camera([](auto& p) { p.far_distance = 0; });
  invalid_camera([](auto& p) { p.far_distance = std::numeric_limits<float>::infinity(); });
  invalid_camera([](auto& p) { p.angle_radians = 3; p.far_distance = std::numeric_limits<float>::max(); });
  invalid_camera([](auto& p) { p.viewport[2] = 0; });
  invalid_camera([](auto& p) { p.viewport[0] = std::numeric_limits<float>::quiet_NaN(); });
  {
    CameraEnabledState state(0x23U);
    rejects([&] { (void)prepare_picture_camera_services(state, parameters, {0, 0, 100, 50}); }, "alternate camera branch rejects");
    check(state.flags() == 0x23U, "unsupported branch preserves canonical flags");
  }
  {
    CameraEnabledState state(0x22U);
    int notifications = 0;
    state.set_enabled(false, true, [&] {
      ++notifications;
      rejects([&] { (void)prepare_picture_camera_services(state, parameters, {0, 0, 100, 50}); },
              "view preparation cannot reenter enabled-state notification");
      check(state.flags() == 0x22U, "rejected nested preparation cannot clear flags");
    });
    (void)prepare_picture_camera_services(state, parameters, {0, 0, 100, 50});
    check(state.flags() == 0 && notifications == 1, "view preparation has no enable-change notification");
  }
  {
    FreshIntroCamera camera(source());
    check(!camera.picture_services(), "fresh camera has no invented prepared view snapshot");
    camera.prepare_picture_services({0, 0, 200, 80});
    check(camera.picture_services()->pass_dimensions == std::array<float, 2>{200, 80} && camera.flags() == 0x80020U,
          "fresh camera owns prepared services without detached runtime flags");
    rejects([&] { camera.prepare_picture_services({0, 0, 0, 80}); }, "failed replacement snapshot rejects");
    check(camera.picture_services()->pass_dimensions == std::array<float, 2>{200, 80}, "failure retains previous snapshot");
    camera.prepare_picture_services({0, 0, 201, 81});
    check(camera.picture_services()->pass_dimensions == std::array<float, 2>{201, 81}, "later successful call refreshes service dimensions");
  }

  const std::array<float, 9> orientation{1, 2, 3, 4, 5, 6, 7, 8, 10};
  std::vector<PictureHierarchyNode> hierarchy{
      {identity, {11, 13, 17}, no_picture_transform_parent},
      {identity, {2, 4, 6}, 0},
      {identity, {5, 7, 9}, 1},
      {orientation, {1, 2, 3}, 1}};
  const auto input = make_intro_picture_cache_input(hierarchy, 3, 2, {-3, 4}, {2, 3}, services, 2.5F);
  check(input.submission_position == std::array<float, 3>{7, 8, 11},
        "root remains in forward chain while terminal camera inverse excludes it");
  check(input.aligned_local_position == std::array<float, 3>{-3, 4, 0} && input.object_matrix == orientation,
        "stored XY is separate from object translation and local orientation remains for later cache step");
  check(input.cached_basis == std::array<float, 9>{53, 32, 14, 128, 77, 32, 213, 128, 53},
        "nonsymmetric local orientation uses recovered transpose-form precombination");
  check(input.virtual_window_scale == services.viewport && input.viewport_width == 100 && input.viewport_height == 50 &&
        input.picture_width == 2 && input.picture_height == 3 && input.external_y_basis_scale == 2.5F &&
        input.owner_projection_scalar == services.projection_scalar,
        "all remaining inputs preserve actual camera services, picture controls and explicit Y policy");
  {
    auto nodes = hierarchy;
    nodes[0].position = {0, 0, 0}; nodes[1].position = {0, 0, 0}; nodes[2].position = {0, 0, 0};
    nodes[3] = {identity, {250, 125, 0}, 1};
    const auto centered = make_intro_picture_cache_input(nodes, 3, 2, {-10, 5}, {1, 1}, services, 1);
    check(centered.submission_position == std::array<float, 3>{250, 125, 0} && centered.cached_basis == identity &&
          centered.aligned_local_position == std::array<float, 3>{-10, 5, 0},
          "Center translation enters exactly once and is not folded into alignment");
    PictureSubmissionCache cache;
    std::array<off::data::BoundPictureDrawGroup, 2> groups;
    groups[0].quads.push_back({.local_z = 2, .modulation_color = 0xff123456U,
        .horizontal_edge_span = 6, .vertical_edge_span = 4});
    groups[1].quads.push_back({.local_z = 3, .modulation_color = 0xffabcdefU,
        .horizontal_edge_span = 2, .vertical_edge_span = 8});
    std::vector<std::size_t> visited;
    cache.submit(std::span<const off::data::BoundPictureDrawGroup>(groups), centered, 37,
        [&](std::size_t i, const auto& group, const auto& transform, std::uint32_t control) {
          visited.push_back(i);
          const auto batches = expand_picture_descriptors(group.quads, transform);
          check(control == 37 && batches.size() == 1 && batches[0].vertices.size() == 4 &&
                batches[0].indices.size() == 6 &&
                batches[0].vertices[0].position[2] == std::array<float, 2>{8, 9}[i] &&
                batches[0].vertices[0].color == std::array<std::uint32_t, 2>{0x7f091a2bU, 0x7f556677U}[i],
                "concrete transform joins ordered cache visitation and descriptor expansion");
        });
    check(visited == std::vector<std::size_t>{0, 1} && !cache.dirty(), "joined submission preserves all group order");
    const auto zero_scale = make_intro_picture_cache_input(nodes, 3, 2, {0, 0}, {0, 0}, services, 0);
    const auto zero_transform = prepare_picture_cache_transform(zero_scale);
    check(zero_transform.translation[0] == 0 && zero_transform.translation[1] == 0,
          "existing zero picture-scale and zero Y-policy behavior is not newly rejected");
  }
  rejects([&] { (void)make_intro_picture_cache_input(hierarchy, 9, 2, {0, 0}, {1, 1}, services, 1); }, "bad live endpoint rejects");
  rejects([&] { (void)make_intro_picture_cache_input(hierarchy, 3, 2, {0, 0}, {-1, 1}, services, 1); }, "negative picture scale rejects");
  rejects([&] { (void)make_intro_picture_cache_input(hierarchy, 3, 2, {0, 0}, {1, 1}, services,
                    std::numeric_limits<float>::quiet_NaN()); }, "undefined operand must have initialized finite native policy");
  for (int rounding : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
    if (std::fesetround(rounding) != 0) continue;
    CameraEnabledState state(2);
    rejects([&] { (void)prepare_picture_camera_services(state, parameters, {0, 0, 100, 50}); }, "camera rejects alternate rounding");
    rejects([&] { (void)make_intro_picture_cache_input(hierarchy, 3, 2, {0, 0}, {1, 1}, services, 1); }, "basis join rejects alternate rounding");
    check(state.flags() == 2, "rounding rejection preserves flags");
  }
  check(std::fesetround(saved_rounding) == 0, "restore caller rounding mode");
  return failures == 0 ? 0 : 1;
}
