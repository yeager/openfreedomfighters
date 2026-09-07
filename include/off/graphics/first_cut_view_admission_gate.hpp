#pragma once

#include "off/graphics/intro_startup_activation.hpp"
#include "off/graphics/renderer_camera_registry.hpp"

#include <cstdint>
#include <functional>

namespace off::graphics {

struct FirstCutViewAdmissionGateServices {
  // The resolver must return the same reference retained by the completed
  // first-cut route; it cannot substitute another live camera.
  std::function<std::optional<FirstCutRequestedCamera>(std::uint64_t)>
      resolve_selected_camera;
  // The callback must validate both the selected reference and its currently
  // live runtime owner before a renderer view can be admitted.
  std::function<bool(std::uint64_t, std::uint64_t)> enabled_camera_matches;
  RendererCameraViewAdmissionServices renderer;
};

enum class FirstCutViewAdmissionResult {
  startup_incomplete,
  deadline_event_not_activated,
  first_cut_route_not_selected,
  renderer_walk_not_visible,
  selected_camera_unavailable,
  selected_camera_not_enabled,
  backend_absent,
  backend_not_ready,
  pending_queued,
  view_admitted,
};

class FirstCutViewAdmissionGate final {
public:
  [[nodiscard]] FirstCutViewAdmissionResult admit(
      IntroStartupActivationStage startup_stage,
      MovieControlEvent16Result event16_result,
      FirstCutRequestedCameraResult camera_result,
      const FirstCutRequestedCameraRoute& camera_route,
      const FirstCutViewAdmissionGateServices& services);
  [[nodiscard]] bool failed() const noexcept { return failed_; }

private:
  RendererCameraViewAdmission renderer_admission_;
  bool busy_{};
  bool failed_{};
};

} // namespace off::graphics
