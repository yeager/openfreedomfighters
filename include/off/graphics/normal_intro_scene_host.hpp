#pragma once

#include "off/graphics/first_cut_view_admission_gate.hpp"
#include "off/platform/first_cut_picture_frame.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {

// Owns only ordering evidence for a future normal first-cut scene. It supplies
// no lifecycle, camera, view, picture, audio, or renderer fallback.
enum class NormalIntroSceneHostStage : std::uint8_t {
  constructed, awaiting_event16, event16_not_activated, first_cut_routed,
  view_pending, view_admitted, frame_assembled, failed,
};

struct NormalIntroSceneHostFirstCutConfig {
  bool first_option{};
  bool optional_renderer_flag{};
  std::uint64_t requested_camera_reference{};
};

class NormalIntroSceneHost final {
 public:
  NormalIntroSceneHost(std::uint64_t movie_component_handle,
                       std::uint64_t movie_owner_handle, std::int32_t movie_delay,
                       IntroStartupActivationBoundaries activation_boundaries,
                       NormalIntroSceneHostFirstCutConfig first_cut);
  void activate(const IntroStartupActivationServices& services);
  [[nodiscard]] MovieControlEvent16Result dispatch_event16(
      const MovieControlEvent16Services& services);
  [[nodiscard]] FirstCutRequestedCameraResult route_first_cut(
      const FirstCutRequestedCameraServices& services);
  [[nodiscard]] FirstCutViewAdmissionResult admit_first_cut_view(
      const FirstCutViewAdmissionGateServices& services);
  [[nodiscard]] platform::FirstCutPictureFrameResult assemble_first_cut_frame(
      const platform::FirstCutPictureFrameInput& evidence);
  [[nodiscard]] NormalIntroSceneHostStage stage() const noexcept { return stage_; }
  [[nodiscard]] const platform::FirstCutPictureFrame& first_cut_frame() const noexcept {
    return frame_;
  }

 private:
  MovieControlFirstUpdate movie_control_;
  IntroStartupActivation activation_;
  FirstCutRequestedCameraRoute route_;
  FirstCutViewAdmissionGate view_gate_;
  platform::FirstCutPictureFrame frame_;
  NormalIntroSceneHostStage stage_{NormalIntroSceneHostStage::constructed};
  std::optional<MovieControlEvent16Result> event_;
  std::optional<FirstCutRequestedCameraResult> route_result_;
  std::optional<FirstCutViewAdmissionResult> view_result_;
};

}  // namespace off::graphics
