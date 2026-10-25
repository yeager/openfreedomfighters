#pragma once

#include "off/graphics/scene_instance_history.hpp"
#include "off/graphics/temporal_resolve_inputs.hpp"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <span>

namespace off::graphics {

// A backend-independent camera record for one admitted temporal scene frame.
// It deliberately contains the two matrices as supplied by the scene
// dispatcher; this contract does not reconstruct a camera, synthesize a
// previous matrix, or apply jitter on the dispatcher's behalf.
struct TemporalSceneCameraTransforms {
  std::array<float, 16> current_view_projection{};
  std::array<float, 16> previous_view_projection{};
  bool previous_valid{};
  bool current_projection_jittered{};
};

// Explicit producer receipts. They are distinct from resource allocation: a
// target with an identity is insufficient until the live scene producer has
// recorded the corresponding data for this frame.
struct TemporalSceneFrameProducerEvidence {
  bool color_written{};
  bool depth_written{};
  bool motion_vectors_written{};
  bool exposure_written{};
  bool reactive_mask_written{};
  bool hudless_color_written{};
  bool camera_transforms_written{};
  bool instance_transforms_written{};
};

// A non-owning input view for a future admitted scene-frame renderer. The
// caller retains the instance records until the backend has consumed this
// view. This is intentionally not connected to the current startup fallback.
struct AdmittedTemporalSceneFrameInput {
  TemporalResolveInputs temporal{};
  TemporalSceneCameraTransforms camera{};
  std::span<const SceneInstanceSubmissionTransform> instances{};
  TemporalSceneFrameProducerEvidence producer_evidence{};
};

// Fail-closed gate for a real temporal scene-frame producer. In addition to
// TemporalResolveInputs readiness it requires finite camera and instance
// transforms, non-empty unique scene identities, dedicated resource
// identities, and producer receipts for every resource. It cannot allocate
// resources, create motion vectors, infer transforms, or enable a vendor SDK.
class AdmittedTemporalSceneFrame final {
public:
  [[nodiscard]] static std::optional<AdmittedTemporalSceneFrame>
  admit(AdmittedTemporalSceneFrameInput input) noexcept;

  [[nodiscard]] const TemporalResolveInputs& temporal() const noexcept {
    return input_.temporal;
  }
  [[nodiscard]] const TemporalSceneCameraTransforms& camera() const noexcept {
    return input_.camera;
  }
  [[nodiscard]] std::span<const SceneInstanceSubmissionTransform>
  instances() const noexcept {
    return input_.instances;
  }

private:
  explicit AdmittedTemporalSceneFrame(AdmittedTemporalSceneFrameInput input) noexcept
      : input_(input) {}

  AdmittedTemporalSceneFrameInput input_;
};

// Synchronous producer-to-backend dispatch for a fully admitted live frame.
// The input and its instance span stay caller-owned, so they must remain valid
// until the callback returns. History is committed only when the backend
// confirms that it accepted the frame for submission; rejection and exceptions
// leave the prior committed transforms intact.
class TemporalSceneFrameDispatcher final {
public:
  using Submit = std::function<bool(const AdmittedTemporalSceneFrame&)>;

  [[nodiscard]] bool dispatch(const AdmittedTemporalSceneFrameInput& input,
                              const Submit& submit);
  void invalidate();
  [[nodiscard]] bool history_valid() const noexcept { return history_valid_; }

private:
  std::map<std::uint64_t, SceneInstanceTransformSnapshot> committed_instances_;
  std::array<float, 16> committed_camera_{};
  bool history_valid_{};
  bool dispatching_{};
};

} // namespace off::graphics
