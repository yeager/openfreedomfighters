#include "off/graphics/normal_intro_scene_host.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
  using namespace off::graphics;
  NormalIntroSceneHost host(17U, 91U, 1, {}, {false, false, 44U});
  bool rejected{};
  try { host.activate({}); } catch (const std::runtime_error&) { rejected = true; }
  check(rejected && host.stage() == NormalIntroSceneHostStage::failed,
        "missing lifecycle boundaries fail before event, view, or frame admission");

  NormalIntroSceneHost queued{17U, 91U, 1,
      {.reader_bracket = [] {}, .outer_loader_tail = [] {},
       .enter_global_lifecycle = [] {}},
      {false, false, 44U}};
  queued.activate({.movie_control_phase_two = {
      [] { return false; }, [] {}, [](bool) {}, [] {}, [] { return 0; }, [] {}}});
  check(queued.dispatch_event16({
      [] { return true; }, [] { return true; }, [] { return false; },
      [] { return std::optional<std::uint64_t>{}; }, [] { return true; },
      [] { return 2; }, [] {}, [](std::uint64_t) {}, [](std::uint64_t) {}}) ==
      MovieControlEvent16Result::activated,
      "queued-view test requires event admission");
  check(queued.route_first_cut({
      [](std::string_view) { return std::optional<std::vector<std::uint8_t>>{}; },
      [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
        return FirstCutRequestedCamera{reference, 99U, 0U, -3, true};
      }, {}, {}, {}, {}, {}, {}, {}, {},
      [](std::uint32_t) {}, [](std::uint64_t, float) { return true; },
      [](std::uint64_t) {}, {}}) ==
      FirstCutRequestedCameraResult::requested_camera_selected,
      "queued-view test requires selected camera route");
  unsigned queue_calls{};
  const auto pending_services = [&] {
    RendererCameraViewAdmissionServices renderer{
        [] { return true; }, [] { return true; },
        [] { return std::optional<RendererViewState>{{7U}}; }, {}, {}, {},
        [](RendererViewState) { return false; },
        [](RendererViewState) { return std::size_t{}; },
        [&queue_calls](RendererViewState, std::uint64_t, std::int32_t) { ++queue_calls; },
        {}, {}, {}, {}, {}, {}, {}};
    return FirstCutViewAdmissionGateServices{
        [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
          return FirstCutRequestedCamera{reference, 99U, 0U, -3, true};
        },
        [](std::uint64_t, std::uint64_t) { return true; }, std::move(renderer)};
  };
  check(queued.admit_first_cut_view(pending_services()) ==
      FirstCutViewAdmissionResult::pending_queued &&
      queued.stage() == NormalIntroSceneHostStage::view_queued && queue_calls == 1U,
      "one pending view is queued exactly once");
  rejected = false;
  try { static_cast<void>(queued.admit_first_cut_view(pending_services())); }
  catch (const std::runtime_error&) { rejected = true; }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::view_queued &&
      queue_calls == 1U,
      "a queued view cannot be queued again before renderer materialization");
  rejected = false;
  try { static_cast<void>(queued.assemble_first_cut_frame({})); }
  catch (const std::runtime_error&) { rejected = true; }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::view_queued,
      "a queued view cannot assemble a frame before materialization and admission");
  std::cout << "normal intro scene host tests passed\n";
}
