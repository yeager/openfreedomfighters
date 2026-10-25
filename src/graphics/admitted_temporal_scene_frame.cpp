#include "off/graphics/admitted_temporal_scene_frame.hpp"

#include <cmath>
#include <stdexcept>

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

bool TemporalSceneFrameDispatcher::dispatch(
    const AdmittedTemporalSceneFrameInput& input, const Submit& submit) {
  if (dispatching_)
    throw std::runtime_error("temporal scene frame dispatch reentrancy is unsupported");
  if (!submit)
    throw std::runtime_error("temporal scene frame submit callback is required");

  std::vector<SceneInstanceSubmissionTransform> prepared_instances(
      input.instances.begin(), input.instances.end());
  AdmittedTemporalSceneFrameInput prepared = input;
  prepared.instances = prepared_instances;

  const auto first_frame = !history_valid_;
  if (first_frame) {
    prepared.camera.previous_view_projection = prepared.camera.current_view_projection;
    prepared.camera.previous_valid = false;
    for (auto& instance : prepared_instances) {
      instance.previous = instance.current;
      instance.previous_valid = false;
    }
  } else {
    for (auto& instance : prepared_instances) {
      const auto previous = committed_instances_.find(instance.identity);
      if (previous != committed_instances_.end()) {
        instance.previous = previous->second;
        instance.previous_valid = true;
      } else {
        instance.previous = instance.current;
        instance.previous_valid = false;
      }
    }
    prepared.camera.previous_view_projection = committed_camera_;
    prepared.camera.previous_valid = true;
  }

  const auto admitted = AdmittedTemporalSceneFrame::admit(prepared);
  if (!admitted)
    return false;

  std::map<std::uint64_t, SceneInstanceTransformSnapshot> next_instances;
  for (const auto& instance : prepared_instances)
    next_instances.emplace(instance.identity, instance.current);

  struct Guard final {
    bool& active;
    explicit Guard(bool& value) : active(value) { active = true; }
    ~Guard() { active = false; }
  } guard(dispatching_);
  if (!submit(*admitted))
    return false;

  committed_instances_ = std::move(next_instances);
  committed_camera_ = prepared.camera.current_view_projection;
  history_valid_ = true;
  return true;
}

void TemporalSceneFrameDispatcher::invalidate() {
  if (dispatching_)
    throw std::runtime_error("temporal scene frame history cannot be invalidated during dispatch");
  committed_instances_.clear();
  committed_camera_ = {};
  history_valid_ = false;
}

} // namespace off::graphics
