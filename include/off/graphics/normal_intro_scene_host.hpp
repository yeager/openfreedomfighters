#pragma once

#include "off/graphics/first_cut_view_admission_gate.hpp"
#include "off/graphics/first_cut_legal_picture_activation_receipt.hpp"
#include "off/platform/first_cut_picture_frame.hpp"

#include <cstdint>
#include <optional>

namespace off::cutscene {
struct FirstCutPlayerPhaseOneServices;
struct FirstCutPlayerSessionPhaseTwoServices;
}

namespace off::graphics {

class MovieControlFirstCutRuntimeHandoff;

// Owns only ordering evidence for a future normal first-cut scene. It supplies
// no lifecycle, camera, view, picture, audio, or renderer fallback.
enum class NormalIntroSceneHostStage : std::uint8_t {
  constructed, awaiting_event16, event16_not_activated, first_cut_routed,
  view_pending, view_queued, view_admitted, frame_assembled, failed,
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
                       NormalIntroSceneHostFirstCutConfig first_cut,
                       MovieControlRuntimeIdentity runtime_identity = {});
  // activation_ retains a reference to movie_control_, and renderer bridges
  // retain this host's address. Keep both bindings stable for the host's life.
  NormalIntroSceneHost(const NormalIntroSceneHost &) = delete;
  NormalIntroSceneHost &operator=(const NormalIntroSceneHost &) = delete;
  NormalIntroSceneHost(NormalIntroSceneHost &&) = delete;
  NormalIntroSceneHost &operator=(NormalIntroSceneHost &&) = delete;
  void activate(const IntroStartupActivationServices& services);
  // This is only for a retained normal scene with a completed reader receipt.
  // It cannot invoke, replace, or replay the reader bracket.
  void activate_after_reader_bracket(
      const IntroPostReaderActivationServices& services);
  [[nodiscard]] MovieControlEvent16Result dispatch_event16(
      const MovieControlEvent16Services& services);
  // Dispatches through this host's exact MovieControlFirstUpdate and uses the
  // checked runtime handoff to run the cold first-cut player's typed lifecycle
  // phases only after MovieControl admits its event 16. The handoff replaces
  // only the supplied CutSequence-start operation; the caller still supplies
  // manager gates, sequence preparation, and group-state service behavior.
  [[nodiscard]] MovieControlEvent16Result dispatch_first_cut_event16(
      MovieControlFirstCutRuntimeHandoff& handoff,
      MovieControlEvent16Services services,
      const cutscene::FirstCutPlayerPhaseOneServices& phase_one,
      const cutscene::FirstCutPlayerSessionPhaseTwoServices& phase_two);
  [[nodiscard]] FirstCutRequestedCameraResult route_first_cut(
      const FirstCutRequestedCameraServices& services);
  [[nodiscard]] FirstCutViewAdmissionResult admit_first_cut_view(
      const FirstCutViewAdmissionGateServices& services);
  void materialize_queued_first_cut_view(
      RendererPendingCameraQueue& queue, RendererViewState state,
      const RendererPendingMaterializationServices& services);
  void retain_first_cut_legal_picture_activation(
      FirstCutLegalPictureActivationReceipt&& activation);
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
  std::optional<RendererPendingCameraLease> pending_view_lease_;
  std::optional<RendererPendingCameraMaterializationReceipt> materialized_view_;
  std::optional<platform::FirstCutFrameAdmissionPermit> frame_admission_permit_;
  std::optional<FirstCutLegalPictureActivationReceipt> legal_picture_activation_;
};

}  // namespace off::graphics
