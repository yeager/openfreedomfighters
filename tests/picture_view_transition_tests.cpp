#include "off/graphics/picture_view_transition.hpp"

#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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
IntroCameraState camera() {
  off::data::GmsIntroCameraSource source{};
  source.near_distance = 2;
  source.far_distance = 80;
  source.angle_degrees = 60;
  source.viewport = {0, 0, 1, 1};
  source.background_rgb = {0x12, 0x34, 0x56};
  source.auxiliary_floats = {0.25F, 0.75F};
  return convert_intro_camera_mode_zero(source);
}
PictureViewFogState initial_fog() {
  return {{0x1234U, 0xff010203U, 0xff040506U, 0x5678U}, false, true, -7, -9};
}
struct Recorder {
  std::vector<std::string> events;
  std::vector<PictureDeviceViewport> viewports;
  std::vector<PictureViewClear> clears;
  std::uint32_t backend_color{0xabcdefU};
  bool backend_enabled{true};
  float backend_start{}, backend_end{};
  std::size_t throw_at{};
  void event(const char* name) {
    events.emplace_back(name);
    if (throw_at != 0 && events.size() == throw_at) throw std::runtime_error("injected backend failure");
  }
  PictureViewTransitionHooks hooks() {
    return {
      [this](const auto& value) { event("viewport"); viewports.push_back(value); },
      [this](bool value) { event("enable"); backend_enabled = value; },
      [this](std::uint32_t value) { event("color"); backend_color = value; },
      [this] { event("vertex"); }, [this] { event("table"); },
      [this](float value) { event("start"); backend_start = value; },
      [this](float value) { event("end"); backend_end = value; },
      [this](const auto& value) { event("clear"); clears.push_back(value); }
    };
  }
};
constexpr PictureStoredViewRectangle rectangle{10, 20, 110, 70};
}

int main() {
  static_assert(!std::is_copy_constructible_v<PictureViewTransition>);
  static_assert(!std::is_move_constructible_v<PictureViewTransition>);
  const auto saved_rounding = std::fegetround();
  if (std::fesetround(FE_TONEAREST) != 0) return 1;
  {
    auto current = camera();
    auto fog = initial_fog();
    std::uint32_t activity = 999;
    PictureViewTransition view;
    Recorder output;
    auto hooks = output.hooks();
    const auto color_hook = hooks.fog_color;
    hooks.fog_color = [&](std::uint32_t color) {
      check(activity == 1 && view.last_clear_frame() == 0 && fog.start == 20 && fog.end == 60 &&
            fog.colors.base_color == color && fog.colors.tracked_color == 0x5678U,
            "configuration CPU writes precede color request without advancing guard or tracked color");
      color_hook(color);
    };
    const auto clear_hook = hooks.clear;
    hooks.clear = [&](const auto& clear) {
      check(view.last_clear_frame() == 1, "clear guard store precedes backend clear");
      clear_hook(clear);
    };
    check(view.last_clear_frame() == 0, "allocated view starts with proven zero guard");
    view.run(1, current, 0x80000U, 0, rectangle, 1, true, activity, fog, hooks);
    check(output.events == std::vector<std::string>{"viewport", "viewport", "enable", "color", "vertex", "table", "start", "end", "clear"},
          "viewport twice, conditional disable, fog configuration and clear preserve exact order");
    check(fog.suppression_latched && !fog.tracked_enabled && !output.backend_enabled &&
          output.backend_color == 0xff123456U && output.backend_start == 20 && output.backend_end == 60,
          "view configuration uses live far/fractions and opaque normalized background");
    check(output.clears.size() == 1 && output.clears[0].color && output.clears[0].depth &&
          output.clears[0].stencil && output.clears[0].packed_color == 0x123456U &&
          output.clears[0].depth_value == 1 && output.clears[0].stencil_value == 0,
          "control zero clears color/depth and available stencil using nonopaque background");
    const auto preserved_tracked = fog.colors.tracked_color;
    const PictureMaterialStateInput material{0, 0, 0, 0, std::nullopt, 1, false};
    const auto requests = resolve_picture_material_state(material, fog.colors);
    check(requests.material.fog_color == 0xff123456U && fog.colors.tracked_color == preserved_tracked,
          "material compares explicit stale tracked color, not equal actual backend color");
    if (requests.material.fog_color) {
      fog.colors.tracked_color = *requests.material.fog_color;
      output.backend_color = *requests.material.fog_color;
    }
    check(!resolve_picture_material_state(material, fog.colors).material.fog_color,
          "applying material tracked replacement suppresses redundant subsequent request");
    output.events.clear();
    view.run(1, current, 0x80000U, 0, rectangle, 1, true, activity, fog, output.hooks());
    check(output.events == std::vector<std::string>{"viewport", "viewport", "color", "vertex", "table", "start", "end"} &&
          output.clears.size() == 1, "repeated same-frame transition still prepares viewport/fog but not clear");
  }
  {
    const auto current = camera();
    auto fog = initial_fog();
    std::uint32_t activity = 55;
    PictureViewTransition first, second;
    Recorder output;
    first.run(17, current, 0x8000U, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(first.last_clear_frame() == 17 && output.clears.empty(), "suppressed clear consumes per-view frame guard");
    first.run(17, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(output.clears.empty(), "removing suppression in same frame does not undo consumed guard");
    second.run(17, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(output.clears.size() == 1, "another view sharing camera has independent clear guard");
    first.run(UINT32_MAX, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    first.run(0, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    first.run(0, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(output.clears.size() == 3 && first.last_clear_frame() == 0 && activity == 0,
          "frame guard uses word equality across wrap rather than monotonic comparison");
    PictureViewTransition fresh;
    fresh.run(0, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(output.clears.size() == 3, "fresh zero guard skips clear at actual frame zero, not forced first draw");
  }
  for (std::uint32_t control : {0U, 5U, 1U, 6U, UINT32_MAX}) {
    for (std::uint32_t background : {0U, 1U, 2U, 0x12345678U}) {
      auto current = camera(); current.background = background;
      auto fog = initial_fog(); Recorder output; PictureViewTransition view;
      std::uint32_t activity = 0;
      view.run(1, current, 0, control, rectangle, 1, false, activity, fog, output.hooks());
      const auto normalized = background == 1 ? 0 : background;
      check(output.clears.size() == 1 && output.clears[0].color == (control != 5) &&
            output.clears[0].depth && !output.clears[0].stencil &&
            output.clears[0].packed_color == (control == 0 ? normalized : 0),
            "ordinary control variants and background exactly-one normalization are distinct");
      check(output.backend_color == (normalized | 0xff000000U), "fog color is opaque independently of clear color");
    }
  }
  for (bool latch : {false, true}) for (bool enabled : {false, true}) {
    for (std::uint32_t flags : {0U, 0x80000U}) for (float global : {0.0F, -0.0F, 1.0F}) {
      auto fog = initial_fog(); fog.suppression_latched = latch; fog.tracked_enabled = enabled;
      Recorder output; output.backend_enabled = enabled;
      PictureViewTransition view; std::uint32_t activity = 0;
      view.run(1, camera(), flags, 0, rectangle, global, false, activity, fog, output.hooks());
      const bool disable = global == 0 || flags != 0;
      const bool request = disable && !latch && enabled;
      check(std::count(output.events.begin(), output.events.end(), "enable") == (request ? 1 : 0),
            "fog disable depends on condition, latch and tracked enabled state");
      check(fog.suppression_latched == (disable || latch) &&
            fog.tracked_enabled == (disable && !latch ? false : enabled),
            "already latched fog preserves tracked enabled, absence never requests enable");
      check(output.backend_enabled == (request ? false : enabled), "omitted fog-enable request preserves backend state");
    }
  }
  {
    auto current = camera(); current.fog_start_fraction = -0.0F; current.fog_end_fraction = -0.0F;
    auto fog = initial_fog(); Recorder output; PictureViewTransition view; std::uint32_t activity = 0;
    view.run(1, current, 0, 0, rectangle, 1, false, activity, fog, output.hooks());
    check(std::signbit(fog.start) && fog.end == current.far_distance,
          "negative-zero start preserved and zero end product replaced with current far");
  }
  const auto viewport_case = [&](PictureStoredViewRectangle r, std::array<float, 4> normalized,
                                 std::array<std::uint32_t, 4> expected) {
    auto current = camera(); current.viewport = normalized;
    auto fog = initial_fog(); Recorder output; PictureViewTransition view; std::uint32_t activity = 0;
    view.run(1, current, 0, 0, r, 1, false, activity, fog, output.hooks());
    check(output.viewports.size() == 2, "two viewport requests survive identical values");
    for (const auto& v : output.viewports)
      check(std::array{v.x, v.y, v.width, v.height} == expected && v.minimum_depth == 0 && v.maximum_depth == 1,
            "viewport uses unsigned staged binary32 math then separate component truncation");
  };
  viewport_case({10, 20, 17, 29}, {0.2F, 0.3F, 0.51F, 0.61F}, {11, 22, 3, 5});
  viewport_case(rectangle, {-2, -3, 2, 3}, {10, 20, 100, 50});
  viewport_case({10, 20, 20, 30}, {2, 3, 1, 1}, {30, 50, 10, 10});
  viewport_case({16777217U, 0, 16777218U, 1}, {0, 0, 1, 1}, {16777216U, 0, 1, 1});
  viewport_case({0x80000000U, 0, 0x80000100U, 1}, {0, 0, 1, 1}, {0x80000000U, 0, 256, 1});

  const auto valid_camera = camera(); // Construct before changing the rounding environment.
  const auto invalid = [&](auto mutate) {
    auto current = valid_camera; auto fog = initial_fog(); Recorder output; PictureViewTransition view;
    std::uint32_t activity = 99, control = 0; auto r = rectangle; float global = 1;
    auto hooks = output.hooks(); mutate(current, r, control, global, hooks);
    rejects([&] { view.run(1, current, 0, control, r, global, false, activity, fog, hooks); }, "invalid view input rejects");
    check(output.events.empty() && activity == 99 && view.last_clear_frame() == 0 &&
          fog.start == -7 && fog.end == -9 && fog.colors.base_color == 0x1234U &&
          !fog.suppression_latched && fog.tracked_enabled,
          "native prevalidation rejects before state writes and backend effects");
  };
  invalid([](auto&, auto&, auto& control, auto&, auto&) { control = 4; });
  invalid([](auto&, auto& r, auto&, auto&, auto&) { r.right = r.left; });
  invalid([](auto&, auto& r, auto&, auto&, auto&) { r.bottom = r.top - 1; });
  invalid([](auto& c, auto&, auto&, auto&, auto&) { c.viewport[2] = 0.001F; });
  invalid([](auto& c, auto&, auto&, auto&, auto&) { c.viewport[3] = -1; });
  invalid([](auto& c, auto&, auto&, auto&, auto&) { c.viewport[0] = std::numeric_limits<float>::infinity(); });
  invalid([](auto&, auto& r, auto&, auto&, auto&) { r = {UINT32_MAX - 1, 0, UINT32_MAX, 1}; });
  invalid([](auto&, auto&, auto&, auto& global, auto&) { global = std::numeric_limits<float>::quiet_NaN(); });
  invalid([](auto& c, auto&, auto&, auto&, auto&) { c.fog_start_fraction = std::numeric_limits<float>::infinity(); });
  invalid([](auto& c, auto&, auto&, auto&, auto&) { c.far_distance = std::numeric_limits<float>::max(); c.fog_end_fraction = 2; });
  invalid([](auto&, auto&, auto&, auto&, auto& hooks) { hooks.clear = {}; });
  for (const auto rounding : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
    if (std::fesetround(rounding) == 0) invalid([](auto&, auto&, auto&, auto&, auto&) {});
  }
  check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding for backend tests");

  for (std::size_t failure = 1; failure <= 9; ++failure) {
    auto fog = initial_fog(); Recorder output; output.throw_at = failure;
    PictureViewTransition view; std::uint32_t activity = 0;
    rejects([&] { view.run(7, camera(), 0x80000U, 0, rectangle, 1, true, activity, fog, output.hooks()); },
            "backend failure propagates");
    check(output.events.size() == failure && activity == 7 && view.last_clear_frame() == (failure == 9 ? 7U : 0U),
          "backend exception preserves exact command prefix and guard timing");
    check(fog.tracked_enabled == (failure < 3) && fog.suppression_latched == (failure >= 4) &&
          fog.colors.base_color == (failure >= 4 ? 0xff123456U : 0x1234U) && fog.colors.tracked_color == 0x5678U,
          "backend exception preserves CPU writes without rolling back or inventing later writes");
    Recorder next;
    view.run(8, camera(), 0x80000U, 0, rectangle, 1, true, activity, fog, next.hooks());
    check(next.clears.size() == 1, "exception releases reentry guard for a separately admitted later frame");
  }
  {
    auto fog = initial_fog(); Recorder output; PictureViewTransition view; std::uint32_t activity = 0;
    auto hooks = output.hooks(); const auto record_viewport = hooks.viewport;
    hooks.viewport = [&](const auto& v) {
      rejects([&] { view.run(99, camera(), 0, 0, rectangle, 1, false, activity, fog, output.hooks()); },
              "same-view hook reentry rejects before nested state writes");
      check(activity == 1 && view.last_clear_frame() == 0, "nested rejection preserves outer transition state");
      record_viewport(v);
    };
    view.run(1, camera(), 0, 0, rectangle, 1, false, activity, fog, hooks);
    check(output.clears.size() == 1 && view.last_clear_frame() == 1, "caught reentry leaves successful outer prefix intact");
  }
  check(std::fesetround(saved_rounding) == 0, "restore caller rounding mode");
  return failures == 0 ? 0 : 1;
}
