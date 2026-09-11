#include "off/graphics/first_cut_view_admission_gate.hpp"

#include <stdexcept>

namespace off::graphics {
namespace {
class BusyGuard final {
public:
  explicit BusyGuard(bool& busy) : busy_(busy) { busy_ = true; }
  ~BusyGuard() { busy_ = false; }

private:
  bool& busy_;
};

[[nodiscard]] FirstCutViewAdmissionResult map_renderer_result(
    RendererCameraViewAdmissionResult result) {
  switch (result) {
  case RendererCameraViewAdmissionResult::backend_absent:
    return FirstCutViewAdmissionResult::backend_absent;
  case RendererCameraViewAdmissionResult::backend_not_ready:
    return FirstCutViewAdmissionResult::backend_not_ready;
  case RendererCameraViewAdmissionResult::pending_queued:
    return FirstCutViewAdmissionResult::pending_queued;
  case RendererCameraViewAdmissionResult::view_admitted:
    return FirstCutViewAdmissionResult::view_admitted;
  }
  throw std::runtime_error("Unknown renderer camera admission result");
}
} // namespace

FirstCutViewAdmissionResult FirstCutViewAdmissionGate::admit(
    IntroStartupActivationStage startup_stage,
    MovieControlEvent16Result event16_result,
    FirstCutRequestedCameraResult camera_result,
    const FirstCutRequestedCameraRoute& camera_route,
    const FirstCutViewAdmissionGateServices& supplied) {
  if (busy_ || failed_)
    throw std::runtime_error("First-cut view admission gate is busy or failed");
  if (!supplied.resolve_selected_camera || !supplied.enabled_camera_matches)
    throw std::runtime_error("First-cut view admission gate requires selected-camera services");

  // The admission executes synchronously. Keep the caller-owned service
  // bundle immutable rather than copying intentionally empty callbacks.
  const auto& services = supplied;
  BusyGuard guard(busy_);
  try {
    if (startup_stage != IntroStartupActivationStage::movie_control_phase_two_complete)
      return FirstCutViewAdmissionResult::startup_incomplete;
    if (event16_result != MovieControlEvent16Result::activated)
      return FirstCutViewAdmissionResult::deadline_event_not_activated;
    if (camera_result != FirstCutRequestedCameraResult::requested_camera_selected)
      return FirstCutViewAdmissionResult::first_cut_route_not_selected;
    if (!camera_route.renderer_walk_can_observe_mutation())
      return FirstCutViewAdmissionResult::renderer_walk_not_visible;
    const auto selected_reference = camera_route.selected_reference();
    if (!selected_reference)
      return FirstCutViewAdmissionResult::selected_camera_unavailable;
    const auto camera = services.resolve_selected_camera(*selected_reference);
    if (!camera || camera->reference != *selected_reference || !camera->runtime_owner ||
        !camera->camera_class)
      return FirstCutViewAdmissionResult::selected_camera_unavailable;
    if (!services.enabled_camera_matches(camera->reference, camera->runtime_owner))
      return FirstCutViewAdmissionResult::selected_camera_not_enabled;
    return map_renderer_result(renderer_admission_.admit(
        camera->runtime_owner, camera->priority, services.renderer));
  } catch (...) {
    failed_ = true;
    throw;
  }
}
} // namespace off::graphics
