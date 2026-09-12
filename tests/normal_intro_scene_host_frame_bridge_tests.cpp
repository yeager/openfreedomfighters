#include "off/platform/normal_intro_scene_host_frame_bridge.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

off::graphics::NormalIntroSceneHost admitted_host() {
  using namespace off::graphics;
  NormalIntroSceneHost host{17, 91, 1,
      {.reader_bracket = [] {}, .outer_loader_tail = [] {},
       .enter_global_lifecycle = [] {}},
      {false, false, 44}};
  host.activate({.movie_control_phase_two = {
      [] { return false; }, [] {}, [](bool) {}, [] {}, [] { return 0; }, [] {}}});
  check(host.dispatch_event16({
      [] { return true; }, [] { return true; }, [] { return false; },
      [] { return std::optional<std::uint64_t>{}; }, [] { return true; },
      [] { return 2; }, [] {}, [](std::uint64_t) {}, [](std::uint64_t) {}}) ==
      MovieControlEvent16Result::activated, "event must be admitted");
  check(host.route_first_cut({
      [](std::string_view) { return std::optional<std::vector<std::uint8_t>>{}; },
      [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
        return FirstCutRequestedCamera{reference, 99, 0, -3, true};
      }, {}, {}, {}, {}, {}, {}, {}, {},
      [](std::uint32_t) {}, [](std::uint64_t, float) { return true; },
      [](std::uint64_t) {}, {}}) ==
      FirstCutRequestedCameraResult::requested_camera_selected,
      "route must select the requested camera");
  RendererCameraViewAdmissionServices renderer{
      [] { return true; }, [] { return true; },
      [] { return std::optional<RendererViewState>{{7}}; }, {}, {}, {},
      [](RendererViewState) { return true; },
      [](RendererViewState) { return std::size_t{}; },
      [](RendererViewState, std::uint64_t, std::int32_t) {},
      [](RendererViewState) { return std::size_t{}; },
      [](RendererViewState, std::uint64_t) { return std::uint64_t{9}; },
      [](std::uint64_t, std::uint64_t) {}, [](std::uint64_t) {},
      [](std::uint64_t, std::int64_t) {}, [](std::uint64_t) {},
      [](RendererViewState) {}};
  check(host.admit_first_cut_view({
      [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
        return FirstCutRequestedCamera{reference, 99, 0, -3, true};
      }, [](std::uint64_t, std::uint64_t) { return true; }, std::move(renderer)}) ==
      FirstCutViewAdmissionResult::view_admitted, "view must be admitted");
  off::data::PictureQuad quad{};
  quad.horizontal_edge_span = quad.vertical_edge_span = 1;
  quad.modulation_color = 0xffffffffU;
  off::data::BoundPictureDrawGroup group{{.image_index = 4}, {quad}};
  off::platform::IntroPictureSubmissionInput picture{
      .groups = std::span(&group, 1),
      .transform = {.basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}}};
  check(host.assemble_first_cut_frame({
      .view_admission = FirstCutViewAdmissionResult::view_admitted,
      .positive_time_member_activated = true,
      .activation_prefix_complete = true,
      .ordered_record_accepted = true,
      .pictures = std::span(&picture, 1)}) ==
      off::platform::FirstCutPictureFrameResult::assembled,
      "host must assemble a lifecycle-admitted frame");
  return host;
}
}  // namespace

int main() {
  using namespace off;
  graphics::NormalIntroSceneHost unstarted{17, 91, 1, {}, {false, false, 44}};
  bool rejected{};
  try { static_cast<void>(platform::NormalIntroSceneHostFrameBridge::bind(unstarted)); }
  catch (const std::runtime_error&) { rejected = true; }
  check(rejected, "unstarted host cannot hand a frame to SDL");

  auto host = admitted_host();
  auto bridge = platform::NormalIntroSceneHostFrameBridge::bind(host);
  check(bridge.draws().size() == 1 && bridge.draws()[0].catalog_image_index == 4,
        "admitted host forwards only its assembled source-backed draw");
  std::cout << "normal intro scene host frame bridge tests passed\n";
}
