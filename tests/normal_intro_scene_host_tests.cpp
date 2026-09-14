#include "off/graphics/normal_intro_scene_host.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main() {
  using namespace off::graphics;
  NormalIntroSceneHost host(17U, 91U, 1, {}, {false, false, 44U});
  bool rejected{};
  try {
    host.activate({});
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && host.stage() == NormalIntroSceneHostStage::failed,
        "missing lifecycle boundaries fail before event, view, or frame "
        "admission");

  IntroPostReaderActivationServices post_reader_services{};
  unsigned phase_two_calls{};
  post_reader_services.movie_control_phase_two = {
      [] { return false; },      [] {}, [](bool) {}, [] {}, [] { return 1U; },
      [&] { ++phase_two_calls; }};
  NormalIntroSceneHost failed_post_reader_tail{
      17U,
      91U,
      1,
      {.reader_bracket = {},
       .outer_loader_tail =
           [] { throw std::runtime_error("tail unavailable"); },
       .enter_global_lifecycle = [] {}},
      {false, false, 44U}};
  rejected = false;
  try {
    failed_post_reader_tail.activate_after_reader_bracket(post_reader_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected &&
            failed_post_reader_tail.stage() ==
                NormalIntroSceneHostStage::failed &&
            phase_two_calls == 0U,
        "post-reader tail failure blocks phase two and fails the host");

  NormalIntroSceneHost failed_post_reader_lifecycle{
      17U,
      91U,
      1,
      {.reader_bracket = {},
       .outer_loader_tail = [] {},
       .enter_global_lifecycle =
           [] { throw std::runtime_error("lifecycle unavailable"); }},
      {false, false, 44U}};
  rejected = false;
  try {
    failed_post_reader_lifecycle.activate_after_reader_bracket(
        post_reader_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected &&
            failed_post_reader_lifecycle.stage() ==
                NormalIntroSceneHostStage::failed &&
            phase_two_calls == 0U,
        "post-reader global lifecycle failure blocks phase two and fails the "
        "host");

  NormalIntroSceneHost queued{17U,
                              91U,
                              1,
                              {.reader_bracket = [] {},
                               .outer_loader_tail = [] {},
                               .enter_global_lifecycle = [] {}},
                              {false, false, 44U}};
  queued.activate(
      {.movie_control_phase_two = {[] { return false; }, [] {}, [](bool) {},
                                   [] {}, [] { return 0; }, [] {}}});
  check(queued.dispatch_event16(
            {[] { return true; }, [] { return true; }, [] { return false; },
             [] { return std::optional<std::uint64_t>{}; }, [] { return true; },
             [] { return 2; }, [] {}, [](std::uint64_t) {},
             [](std::uint64_t) {}}) == MovieControlEvent16Result::activated,
        "queued-view test requires event admission");
  check(queued.route_first_cut(
            {[](std::string_view) {
               return std::optional<std::vector<std::uint8_t>>{};
             },
             [](std::uint64_t reference)
                 -> std::optional<FirstCutRequestedCamera> {
               return FirstCutRequestedCamera{reference, 99U, 0U, -3, true};
             },
             {},
             {},
             {},
             {},
             {},
             {},
             {},
             {},
             [](std::uint32_t) {},
             [](std::uint64_t, float) { return true; },
             [](std::uint64_t) {},
             {}}) == FirstCutRequestedCameraResult::requested_camera_selected,
        "queued-view test requires selected camera route");
  unsigned queue_calls{};
  RendererPendingCameraQueue pending_queue;
  const auto pending_services = [&] {
    RendererCameraViewAdmissionServices renderer{
        [] { return true; },
        [] { return true; },
        [] { return std::optional<RendererViewState>{{7U}}; },
        {},
        {},
        {},
        [](RendererViewState) { return false; },
        [](RendererViewState) { return std::size_t{}; },
        [&queue_calls, &pending_queue](RendererViewState state, std::uint64_t camera, std::int32_t priority) {
          ++queue_calls;
          return pending_queue.append(state, camera, priority);
        },
        {},
        {},
        {},
        {},
        {},
        {},
        {}};
    return FirstCutViewAdmissionGateServices{
        [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
          return FirstCutRequestedCamera{reference, 99U, 0U, -3, true};
        },
        [](std::uint64_t, std::uint64_t) { return true; }, std::move(renderer)};
  };
  check(queued.admit_first_cut_view(pending_services()) ==
                FirstCutViewAdmissionResult::pending_queued &&
            queued.stage() == NormalIntroSceneHostStage::view_queued &&
            queue_calls == 1U,
        "one pending view is queued exactly once");
  rejected = false;
  try {
    static_cast<void>(queued.admit_first_cut_view(pending_services()));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::view_queued &&
            queue_calls == 1U,
        "a queued view cannot be queued again before renderer materialization");
  rejected = false;
  try {
    static_cast<void>(queued.assemble_first_cut_frame({}));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::view_queued,
        "a queued view cannot assemble a frame before materialization and "
        "admission");
  unsigned materialized{};
  queued.materialize_queued_first_cut_view(
      pending_queue, {7U},
      {[](RendererViewState) { return true; },
       [&materialized](std::uint64_t camera, std::int32_t priority) {
         check(camera == 99U && priority == -3, "queued materialization preserves selected camera identity");
         ++materialized;
       }});
  check(queued.stage() == NormalIntroSceneHostStage::view_admitted && materialized == 1U &&
            pending_queue.entries().empty(),
        "only the queue-issued pending lease can materialize the selected view once");
  rejected = false;
  try {
    queued.materialize_queued_first_cut_view(
        pending_queue, {7U}, {[](RendererViewState) { return true; }, [](std::uint64_t, std::int32_t) {}});
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::view_admitted,
        "materialized queue receipt cannot be replayed through the host");

  off::data::PictureQuad quad{};
  quad.horizontal_edge_span = quad.vertical_edge_span = 1;
  quad.modulation_color = 0xffffffffU;
  off::data::BoundPictureDrawGroup group{{.image_index = 4U}, {quad}};
  off::platform::IntroPictureSubmissionInput picture{
      .groups = std::span(&group, 1),
      .transform = {.basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}}};
  check(queued.assemble_first_cut_frame(
            {.positive_time_member_activated = true,
             .activation_prefix_complete = true,
             .ordered_record_accepted = true,
             .pictures = std::span(&picture, 1)}) ==
            off::platform::FirstCutPictureFrameResult::assembled &&
            queued.stage() == NormalIntroSceneHostStage::frame_assembled,
        "only the materialized queue receipt mints the one-shot frame permit");
  rejected = false;
  try {
    static_cast<void>(queued.assemble_first_cut_frame({}));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected && queued.stage() == NormalIntroSceneHostStage::frame_assembled,
        "the consumed materialized-view permit cannot assemble a second frame");
  std::cout << "normal intro scene host tests passed\n";
}
