#include "off/graphics/first_cut_view_admission_gate.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace off::graphics;

void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void select_first_cut_camera(FirstCutRequestedCameraRoute& route) {
  const auto result = route.run({
      [](std::string_view) { return std::optional<std::vector<std::uint8_t>>{}; },
      [](std::uint64_t reference) -> std::optional<FirstCutRequestedCamera> {
        return FirstCutRequestedCamera{reference, 99, 0, -3, true};
      },
      {}, {}, {}, {}, {}, {}, {}, {},
      [](std::uint32_t) {},
      [](std::uint64_t, float) { return true; },
      [](std::uint64_t) {}, {}});
  check(result == FirstCutRequestedCameraResult::requested_camera_selected,
        "test route must select its requested camera");
}

struct Harness {
  bool enabled{true};
  bool backend{};
  bool backend_ready{};
  bool state_ready{};
  std::optional<FirstCutRequestedCamera> resolved{FirstCutRequestedCamera{44, 99, 0, -3, true}};
  std::vector<std::string> effects;

  [[nodiscard]] FirstCutViewAdmissionGateServices services() {
    RendererCameraViewAdmissionServices renderer{
        [&] { effects.push_back("backend"); return backend; },
        [&] { effects.push_back("backend-ready"); return backend_ready; },
        [&] { effects.push_back("state"); return std::optional<RendererViewState>{{7}}; },
        {}, {}, {},
        [&](RendererViewState) { effects.push_back("state-ready"); return state_ready; },
        [](RendererViewState) { return std::size_t{}; },
        [&](RendererViewState, std::uint64_t, std::int32_t) { effects.push_back("pending"); },
        [](RendererViewState) { return std::size_t{}; },
        [&](RendererViewState, std::uint64_t) { effects.push_back("allocate"); return std::uint64_t{9}; },
        [&](std::uint64_t, std::uint64_t) { effects.push_back("associate"); },
        [&](std::uint64_t) { effects.push_back("records"); },
        [&](std::uint64_t, std::int64_t) { effects.push_back("insert"); },
        [&](std::uint64_t) { effects.push_back("use"); },
        [&](RendererViewState) { effects.push_back("renumber"); }};
    return {
        [&](std::uint64_t reference) {
          effects.push_back("resolve:" + std::to_string(reference));
          return resolved;
        },
        [&](std::uint64_t reference, std::uint64_t owner) {
          effects.push_back("enabled:" + std::to_string(reference) + ":" + std::to_string(owner));
          return enabled;
        },
        std::move(renderer)};
  }
};

FirstCutViewAdmissionResult admit(FirstCutViewAdmissionGate& gate, Harness& h,
                                  IntroStartupActivationStage stage,
                                  MovieControlEvent16Result event,
                                  FirstCutRequestedCameraResult route_result,
                                  const FirstCutRequestedCameraRoute& route) {
  return gate.admit(stage, event, route_result, route, h.services());
}
} // namespace

int main() {
  try {
    FirstCutRequestedCameraRoute selected{false, false, 44};
    select_first_cut_camera(selected);
    constexpr auto active_stage = IntroStartupActivationStage::movie_control_phase_two_complete;
    constexpr auto activated = MovieControlEvent16Result::activated;
    constexpr auto selected_result = FirstCutRequestedCameraResult::requested_camera_selected;
    {
      Harness h; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, IntroStartupActivationStage::global_lifecycle_complete, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::startup_incomplete && h.effects.empty(),
            "only completed startup activation can reach a first-cut view");
    }
    {
      Harness h; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, MovieControlEvent16Result::waiting_for_deadline, selected_result, selected) ==
                FirstCutViewAdmissionResult::deadline_event_not_activated && h.effects.empty(),
            "deadline result must be the actual activated event");
    }
    {
      Harness h; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, FirstCutRequestedCameraResult::named_camera_route, selected) ==
                FirstCutViewAdmissionResult::first_cut_route_not_selected && h.effects.empty(),
            "named MainCamera route cannot borrow zero-route admission");
    }
    {
      Harness h; FirstCutViewAdmissionGate gate;
      FirstCutRequestedCameraRoute unselected{false, false, 44};
      check(admit(gate, h, active_stage, activated, selected_result, unselected) ==
                FirstCutViewAdmissionResult::renderer_walk_not_visible && h.effects.empty(),
            "route mutation must be visible to the renderer walk");
    }
    {
      Harness h; h.resolved = FirstCutRequestedCamera{45, 99, 0, -3, true}; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::selected_camera_unavailable &&
                h.effects == std::vector<std::string>{"resolve:44"},
            "selected reference cannot be substituted by another camera");
    }
    {
      Harness h; h.enabled = false; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::selected_camera_not_enabled &&
                h.effects == std::vector<std::string>{"resolve:44", "enabled:44:99"},
            "enabled callback must match selected reference and live owner");
    }
    {
      Harness h; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::backend_absent && h.effects.back() == "backend",
            "backend absence remains distinct from queueing");
    }
    {
      Harness h; h.backend = true; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::backend_not_ready && h.effects.back() == "backend-ready",
            "unready backend remains distinct from queueing");
    }
    {
      Harness h; h.backend = true; h.backend_ready = true; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::pending_queued && h.effects.back() == "pending",
            "non-ready state reports a pending view");
    }
    {
      Harness h; h.backend = true; h.backend_ready = true; h.state_ready = true; FirstCutViewAdmissionGate gate;
      check(admit(gate, h, active_stage, activated, selected_result, selected) ==
                FirstCutViewAdmissionResult::view_admitted && h.effects.back() == "renumber",
            "ready state reports a fully admitted view");
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
