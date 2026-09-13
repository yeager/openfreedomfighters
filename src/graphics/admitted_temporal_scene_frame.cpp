#include "off/graphics/admitted_temporal_scene_frame.hpp"

#include <cmath>

namespace off::graphics {
namespace {

[[nodiscard]] bool finite(const std::array<float, 16>& matrix) noexcept {
  for (const float value : matrix) {
    if (!std::isfinite(value))
      return false;
  }
  return true;
}

[[nodiscard]] bool finite(const SceneInstanceTransformSnapshot& transform) noexcept {
  for (const float value : transform.source_basis) {
    if (!std::isfinite(value))
      return false;
  }
  for (const float value : transform.source_position) {
    if (!std::isfinite(value))
      return false;
  }
  for (const float value : transform.map_orientation) {
    if (!std::isfinite(value))
      return false;
  }
  for (const float value : transform.map_position) {
    if (!std::isfinite(value))
      return false;
  }
  return true;
}

[[nodiscard]] bool resources_are_dedicated(const TemporalResolveInputs& inputs) noexcept {
  const std::array resources{inputs.color_resource, inputs.depth_resource,
                             inputs.motion_vector_resource, inputs.exposure_resource,
                             inputs.reactive_mask_resource, inputs.hudless_color_resource,
                             inputs.history_resource};
  for (std::size_t index = 0; index < resources.size(); ++index) {
    if (resources[index] == 0U)
      return false;
    for (std::size_t other = index + 1; other < resources.size(); ++other) {
      if (resources[index] == resources[other])
        return false;
    }
  }
  return true;
}

[[nodiscard]] bool valid_instances(
    std::span<const SceneInstanceSubmissionTransform> instances) noexcept {
  if (instances.empty())
    return false;
  for (std::size_t index = 0; index < instances.size(); ++index) {
    const auto& instance = instances[index];
    if (instance.identity == 0U || !finite(instance.current) ||
        !finite(instance.previous) ||
        (!instance.previous_valid && instance.previous != instance.current))
      return false;
    for (std::size_t other = index + 1; other < instances.size(); ++other) {
      if (instance.identity == instances[other].identity)
        return false;
    }
  }
  return true;
}

[[nodiscard]] bool complete(const AdmittedTemporalSceneFrameInput& input) noexcept {
  const auto& evidence = input.producer_evidence;
  if (!evidence.color_written || !evidence.depth_written ||
      !evidence.motion_vectors_written || !evidence.exposure_written ||
      !evidence.reactive_mask_written || !evidence.hudless_color_written ||
      !evidence.camera_transforms_written || !evidence.instance_transforms_written ||
      !input.temporal.motion_vectors_written || !input.temporal.jitter_applied ||
      !input.camera.current_projection_jittered ||
      !finite(input.camera.current_view_projection) ||
      !finite(input.camera.previous_view_projection) ||
      (!input.camera.previous_valid &&
       input.camera.previous_view_projection != input.camera.current_view_projection))
    return false;
  // Reuse the resolver's exact extent, jitter-coordinate, and history-slot
  // validation rather than duplicating a weaker version at this outer gate.
  TemporalResolveInputLifecycle temporal_inputs;
  if (!temporal_inputs.begin(input.temporal))
    return false;
  temporal_inputs.cancel();
  return resources_are_dedicated(input.temporal) && valid_instances(input.instances);
}

} // namespace

std::optional<AdmittedTemporalSceneFrame>
AdmittedTemporalSceneFrame::admit(AdmittedTemporalSceneFrameInput input) noexcept {
  if (!complete(input))
    return std::nullopt;
  return AdmittedTemporalSceneFrame{input};
}

} // namespace off::graphics
