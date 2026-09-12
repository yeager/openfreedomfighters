#include "off/graphics/temporal_resolve_inputs.hpp"

#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

off::graphics::TemporalResolveInputs complete_inputs() {
  using namespace off::graphics;
  TemporalResolveInputs inputs;
  inputs.internal_extent = {960, 540};
  inputs.output_extent = {1920, 1080};
  inputs.color_resource = 1;
  inputs.depth_resource = 2;
  inputs.motion_vector_resource = 3;
  inputs.exposure_resource = 4;
  inputs.reactive_mask_resource = 5;
  inputs.hudless_color_resource = 6;
  inputs.history_resource = 7;
  inputs.motion_vectors_written = true;
  inputs.jitter_applied = true;
  inputs.history_frame = {.history_slot = 0, .output_slot = 1};
  inputs.jitter_internal_extent = {960, 540};
  inputs.jitter_output_extent = {1920, 1080};
  return inputs;
}
} // namespace

int main() {
  using off::graphics::TemporalResolveInputLifecycle;
  TemporalResolveInputLifecycle readiness;
  auto inputs = complete_inputs();

  check(readiness.begin(inputs),
        "a complete producer-backed input set opens one resolve transaction");
  check(readiness.ready() && readiness.in_flight() && !readiness.begin(inputs),
        "a pending resolve cannot be replaced or overlap another frame");
  check(readiness.commit() && !readiness.ready() && !readiness.in_flight(),
        "commit consumes the ready input set");

  inputs.motion_vectors_written = false;
  check(!readiness.begin(inputs),
        "an allocated but unwritten motion target cannot enable a resolve");
  inputs = complete_inputs();
  inputs.exposure_resource = 0;
  check(!readiness.begin(inputs), "missing exposure fails closed");
  inputs = complete_inputs();
  inputs.jitter_applied = false;
  check(!readiness.begin(inputs), "unapplied jitter fails closed");
  inputs = complete_inputs();
  inputs.jitter_output_extent.width = 1280;
  check(!readiness.begin(inputs), "mismatched jitter coordinates fail closed");
  inputs = complete_inputs();
  inputs.history_frame.output_slot = inputs.history_frame.history_slot;
  check(!readiness.begin(inputs), "invalid history ping-pong slots fail closed");

  inputs = complete_inputs();
  inputs.history_frame.history_valid = false;
  check(readiness.begin(inputs),
        "a first frame may resolve without prior content but needs its target");
  readiness.cancel();
  check(!readiness.ready() && !readiness.in_flight(),
        "cancellation drops all pending resource identities");
  check(!readiness.commit(), "no resolve can commit after cancellation");
  return failures == 0 ? 0 : 1;
}
