#include "off/graphics/renderer_frame.hpp"
#include "off/graphics/renderer_frame_pass.hpp"

#include <array>
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
RendererFrameLifecycleHooks quiet(bool admitted = true) {
  return {[admitted] { return admitted; }, [] {}, [] {}, [] {}, [] {}};
}
struct Trace {
  RendererFrameClock& clock;
  std::vector<std::string> events;
  std::vector<std::uint32_t> words;
  std::size_t throw_at{};
  explicit Trace(RendererFrameClock& current) : clock(current) {}
  void event(const char* name) {
    events.emplace_back(name); words.push_back(clock.value());
    if (throw_at != 0 && events.size() == throw_at) throw std::runtime_error("injected renderer callback failure");
  }
  RendererFrameLifecycleHooks hooks(bool admitted = true) {
    return {[this, admitted] { event("admit"); return admitted; },
            [this] { event("traverse"); }, [this] { event("post"); },
            [this] { event("end"); }, [this] { event("complete"); }};
  }
};
}

int main() {
  static_assert(!std::is_copy_constructible_v<RendererFrameClock>);
  static_assert(!std::is_move_constructible_v<RendererFrameClock>);
  static_assert(!std::is_copy_constructible_v<RendererFrame>);
  static_assert(!std::is_move_constructible_v<RendererFrame>);
  const std::vector<std::string> success{"admit", "traverse", "post", "end", "complete"};
  {
    RendererFrameClock clock; RendererFrame renderer; Trace trace(clock);
    check(clock.value() == 1, "fresh shared engine frame word is one");
    check(renderer.run(clock, true, true, trace.hooks()) == RendererFrameOutcome::rendered &&
          trace.events == success && trace.words == std::vector<std::uint32_t>(5, 1) && clock.value() == 2,
          "admitted frame traverses, performs post work, ends scene and completes before increment");
    Trace failed(clock);
    check(renderer.run(clock, true, true, failed.hooks(false)) == RendererFrameOutcome::admission_failed &&
          failed.events == std::vector<std::string>{"admit", "complete"} &&
          failed.words == std::vector<std::uint32_t>{2, 2} && clock.value() == 3,
          "failed device admission skips rendering but still completes and advances shared word");
  }
  for (bool running : {false, true}) for (bool initialized : {false, true}) {
    if (running && initialized) continue;
    RendererFrameClock clock(71); RendererFrame renderer; Trace trace(clock);
    check(renderer.run(clock, running, initialized, trace.hooks()) == RendererFrameOutcome::skipped &&
          trace.events.empty() && clock.value() == 71, "either failed outer gate does nothing");
    check(renderer.run(clock, running, initialized, {}) == RendererFrameOutcome::skipped && clock.value() == 71,
          "failed gates need no backend hooks or fabricated admission");
  }
  {
    RendererFrameClock clock; RendererFrame first, second;
    std::vector<std::uint32_t> observed;
    auto first_hooks = quiet(); first_hooks.backend_traversal = [&] { observed.push_back(clock.value()); };
    auto second_hooks = quiet(false); second_hooks.renderer_completion = [&] { observed.push_back(clock.value()); };
    (void)first.run(clock, true, true, first_hooks);
    (void)second.run(clock, true, true, second_hooks);
    (void)first.run(clock, true, true, first_hooks);
    check(observed == std::vector<std::uint32_t>{1, 2, 3} && clock.value() == 4,
          "separate renderer coordinators consume the same engine clock including failed admission");
    // No renderer invocation corresponds to a non-rendering update.
    check(clock.value() == 4, "clock has no independent application-update tick");
  }
  {
    RendererFrameClock clock(std::numeric_limits<std::uint32_t>::max()); RendererFrame renderer;
    Trace first(clock), next(clock);
    (void)renderer.run(clock, true, true, first.hooks());
    check(clock.value() == 0 && first.words == std::vector<std::uint32_t>(5, UINT32_MAX),
          "successful completion increments unsigned maximum to zero after all callbacks");
    (void)renderer.run(clock, true, true, next.hooks(false));
    check(clock.value() == 1 && next.words == std::vector<std::uint32_t>{0, 0},
          "failed admission also advances zero without inventing a monotonic signed counter");
  }
  {
    RendererFrameClock clock; RendererFrame renderer;
    for (unsigned missing = 0; missing < 5; ++missing) {
      Trace trace(clock); auto hooks = trace.hooks(false);
      switch (missing) {
        case 0: hooks.admit_device_scene = {}; break;
        case 1: hooks.backend_traversal = {}; break;
        case 2: hooks.admitted_post_render = {}; break;
        case 3: hooks.end_scene = {}; break;
        case 4: hooks.renderer_completion = {}; break;
      }
      rejects([&] { (void)renderer.run(clock, true, true, hooks); }, "incomplete admitted lifecycle hooks reject");
      check(clock.value() == 1 && trace.events.empty(), "native hook validation precedes admission and all effects");
    }
  }
  for (const bool admitted : {true, false}) {
    const auto expected = admitted ? success : std::vector<std::string>{"admit", "complete"};
    for (std::size_t fail = 1; fail <= expected.size(); ++fail) {
      RendererFrameClock clock(37); RendererFrame renderer; Trace trace(clock); trace.throw_at = fail;
      rejects([&] { (void)renderer.run(clock, true, true, trace.hooks(admitted)); }, "callback failure propagates");
      check(trace.events == std::vector<std::string>(expected.begin(), expected.begin() + static_cast<std::ptrdiff_t>(fail)) &&
            trace.words == std::vector<std::uint32_t>(fail, 37) && clock.value() == 37,
            "failure retains exact prefix without invented completion or frame increment");
      (void)renderer.run(clock, false, false, {});
      RendererFrame another;
      check(another.run(clock, true, true, quiet(false)) == RendererFrameOutcome::admission_failed && clock.value() == 38,
            "exception releases both coordinator and shared-clock guards for a separately admitted operation");
    }
  }
  {
    RendererFrameClock clock, independent_clock; RendererFrame outer, nested;
    Trace trace(clock); auto hooks = trace.hooks(); const auto traversal = hooks.backend_traversal;
    hooks.backend_traversal = [&] {
      rejects([&] { (void)outer.run(clock, true, true, quiet()); }, "same coordinator cannot reenter");
      rejects([&] { (void)nested.run(clock, true, true, quiet()); }, "second coordinator cannot nest on same engine clock");
      rejects([&] { (void)nested.run(clock, false, false, {}); }, "shared-clock reentry rejects even before failed nested gates");
      rejects([&] { (void)outer.run(independent_clock, true, true, quiet()); }, "same coordinator rejects nested use with another clock");
      check(clock.value() == 1 && independent_clock.value() == 1, "nested rejection changes neither engine word");
      traversal();
    };
    check(outer.run(clock, true, true, hooks) == RendererFrameOutcome::rendered &&
          trace.events == success && clock.value() == 2, "caught nested attempts preserve outer callback order and single increment");
  }
  {
    RendererFrameClock clock; RendererFrame outer; RendererFramePass inner;
    std::vector<std::string> order;
    const std::array<RendererStateEntry, 2> states{{{101, 201}, {101, 202}}};
    const RendererFrameHooks inner_hooks{
      [&](auto state) { check(clock.value() == 1, "inner state frame observes current outer engine word"); order.push_back("frame:" + std::to_string(state)); },
      [&](auto state) { order.push_back("maintenance:" + std::to_string(state)); },
      [&] { order.push_back("backend-maintenance"); }};
    const RendererFrameLifecycleHooks hooks{
      [] { return true; }, [&] { inner.run(true, true, 101, states, inner_hooks); },
      [&] { order.push_back("post"); }, [&] { order.push_back("end"); }, [&] { order.push_back("completion"); }};
    (void)outer.run(clock, true, true, hooks);
    check(order == std::vector<std::string>{"frame:201", "frame:202", "maintenance:201", "maintenance:202",
          "backend-maintenance", "post", "end", "completion"} && clock.value() == 2,
          "outer coordinator can retain existing inner backend traversal without collapsing its distinct gates");
  }
  {
    OrdinarySceneUpdate update;
    LiveRendererNode third{3, nullptr}, second{2, &third}, first{1, &second};
    std::vector<std::string> order;
    const OrdinarySceneUpdateHooks hooks{
      [&] { order.push_back("scheduled"); }, [&] { order.push_back("ordinary"); },
      [&] { order.push_back("bounds"); }, [&] { order.push_back("maintenance"); },
      [&](LiveRendererNode& node) {
        order.push_back("renderer:" + std::to_string(node.identity));
        if (node.identity == 1) node.next = &third;
      }, [&] { order.push_back("post"); }};
    update.run(true, &first, hooks);
    check(order == std::vector<std::string>{"scheduled", "ordinary", "bounds", "maintenance",
          "renderer:1", "renderer:3", "post"},
          "ordinary update preserves work order and reads next renderer after its callback");
    order.clear(); update.run(false, &first, hooks);
    check(order == std::vector<std::string>{"scheduled", "ordinary", "bounds", "maintenance"},
          "render false suppresses only linked renderer traversal and post-render work");
  }
  {
    RendererFrameClock clock; EligibleRendererFrame frame; EligibleRendererFrameState state{8, 9};
    std::vector<std::string> order;
    const EligibleRendererFrameHooks hooks{
      [&] { order.push_back("suppressed"); return false; },
      [&] { order.push_back("ready"); return true; }, [&] { order.push_back("begin"); return false; },
      [&] { order.push_back("backend"); }, [&] { order.push_back("post"); },
      [&] { order.push_back("end"); }, [&] { order.push_back("complete"); }};
    check(frame.run(clock, state, true, true, hooks) == RendererFrameOutcome::admission_failed &&
          state.local_counter_a == 0 && state.local_counter_b == 0 && !state.in_scene && clock.value() == 2 &&
          order == std::vector<std::string>{"suppressed", "ready", "begin", "complete"},
          "eligible device failure resets local counters but still completes and advances the engine word");
    state = {4, 5}; order.clear();
    check(frame.run(clock, state, false, true, hooks) == RendererFrameOutcome::skipped &&
          state.local_counter_a == 4 && state.local_counter_b == 5 && !state.in_scene && clock.value() == 2 && order.empty(),
          "failed initialized gate neither resets counters nor calls device services");
  }
  {
    BackendTraversal traversal;
    const std::array<BackendAdmittedView, 3> first_views{{{10, 100}, {11, std::nullopt}, {12, 102}}};
    const std::array<BackendAdmittedView, 1> second_views{{{20, 200}}};
    const std::array<BackendStateInput, 3> states{{{9, 90, first_views}, {9, 91, second_views}, {8, 80, {}}}};
    std::vector<std::string> order;
    unsigned rounds{};
    const BackendTraversalHooks hooks{
      [&](auto state) { order.push_back("state:" + std::to_string(state)); },
      [](auto camera) { return camera != 102; },
      [&](auto camera, auto view) { order.push_back("transform:" + std::to_string(camera) + ":" + std::to_string(view)); },
      [&](auto state, auto view) { order.push_back("begin:" + std::to_string(state) + ":" + std::to_string(view)); },
      [&](auto state, auto view) { order.push_back("view:" + std::to_string(state) + ":" + std::to_string(view)); },
      [&](auto state, auto view) { order.push_back("end-view:" + std::to_string(state) + ":" + std::to_string(view)); },
      [&](auto state) { order.push_back("maintenance:" + std::to_string(state)); },
      [&] { order.push_back("prepare"); },
      [&](auto retained) { order.push_back("preselect:" + std::to_string(retained.size())); },
      [&](auto state) { order.push_back("draw:" + std::to_string(state)); return rounds++ == 0; },
      [&] { order.push_back("restore"); }};
    traversal.run(9, states, hooks);
    check(order == std::vector<std::string>{"state:90", "transform:100:10", "begin:90:10", "view:90:10", "end-view:90:10",
          "state:91", "transform:200:20", "begin:91:20", "view:91:20", "end-view:91:20",
          "maintenance:90", "maintenance:91", "prepare", "preselect:2", "draw:90", "draw:91", "draw:90", "draw:91", "restore"},
          "backend consumes one matching state snapshot, indexed live views, then whole-state drawing rounds");
  }
  return failures == 0 ? 0 : 1;
}
