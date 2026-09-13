#include "off/graphics/admitted_temporal_scene_frame.hpp"

#include <iostream>
#include <vector>

namespace {
int failures{};
void check(bool value, const char* message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

off::graphics::AdmittedTemporalSceneFrameInput complete_input(
    std::vector<off::graphics::SceneInstanceSubmissionTransform>& instances) {
  using namespace off::graphics;
  AdmittedTemporalSceneFrameInput input;
  input.temporal.internal_extent = {960, 540};
  input.temporal.output_extent = {1920, 1080};
  input.temporal.color_resource = 1;
  input.temporal.depth_resource = 2;
  input.temporal.motion_vector_resource = 3;
  input.temporal.exposure_resource = 4;
  input.temporal.reactive_mask_resource = 5;
  input.temporal.hudless_color_resource = 6;
  input.temporal.history_resource = 7;
  input.temporal.motion_vectors_written = true;
  input.temporal.jitter_applied = true;
  input.temporal.history_frame = {.history_slot = 0, .output_slot = 1};
  input.temporal.jitter_internal_extent = {960, 540};
  input.temporal.jitter_output_extent = {1920, 1080};
  input.camera.current_view_projection[0] = 1.0F;
  input.camera.current_view_projection[5] = 1.0F;
  input.camera.current_view_projection[10] = 1.0F;
  input.camera.current_view_projection[15] = 1.0F;
  input.camera.previous_view_projection = input.camera.current_view_projection;
  input.camera.current_projection_jittered = true;
  input.producer_evidence = {.color_written = true,
                             .depth_written = true,
                             .motion_vectors_written = true,
                             .exposure_written = true,
                             .reactive_mask_written = true,
                             .hudless_color_written = true,
                             .camera_transforms_written = true,
                             .instance_transforms_written = true};
  SceneInstanceTransformSnapshot transform;
  transform.source_basis[0] = 1.0F;
  transform.source_basis[4] = 1.0F;
  transform.source_basis[8] = 1.0F;
  transform.map_orientation = transform.source_basis;
  instances = {{.identity = 1, .current = transform, .previous = transform}};
  input.instances = instances;
  return input;
}
} // namespace

int main() {
  using off::graphics::AdmittedTemporalSceneFrame;
  std::vector<off::graphics::SceneInstanceSubmissionTransform> instances;
  auto input = complete_input(instances);
  const auto admitted = AdmittedTemporalSceneFrame::admit(input);
  check(admitted.has_value(), "complete live-scene receipts are admitted");
  check(admitted && admitted->instances().data() == instances.data(),
        "the contract retains a non-owning instance view");

  input = complete_input(instances);
  input.producer_evidence.depth_written = false;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "allocated resources without a producer receipt are rejected");
  input = complete_input(instances);
  input.temporal.color_resource = input.temporal.depth_resource;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "aliased resource identities cannot impersonate dedicated inputs");
  input = complete_input(instances);
  input.temporal.jitter_output_extent.width = 1280;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "a mismatched temporal coordinate contract is rejected");
  input = complete_input(instances);
  input.camera.current_projection_jittered = false;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "an unjittered camera cannot enter the temporal path");
  input = complete_input(instances);
  input.camera.previous_view_projection[12] = 1.0F;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "a first-frame fabricated previous camera transform is rejected");
  input = complete_input(instances);
  instances[0].previous.source_position[0] = 1.0F;
  check(!AdmittedTemporalSceneFrame::admit(input),
        "a first-frame fabricated previous instance transform is rejected");
  input = complete_input(instances);
  input.instances = {};
  check(!AdmittedTemporalSceneFrame::admit(input),
        "a static fallback with no scene instances cannot enter the temporal path");
  return failures == 0 ? 0 : 1;
}
