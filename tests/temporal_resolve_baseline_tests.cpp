#include "off/graphics/temporal_resolve_baseline.hpp"

#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

off::graphics::TemporalResolveResourceSet resources() {
  return {.color = 1,
          .depth = 2,
          .motion_vectors = 3,
          .exposure = 4,
          .reactive_mask = 5,
          .hudless_color = 6,
          .history = {7, 8},
          .motion_vectors_written = true};
}
} // namespace

int main() {
  using namespace off::graphics;
  TemporalResolveBaseline baseline;
  check(!baseline.configure({}), "invalid history target is rejected");
  check(baseline.configure({1920, 1080, 1}),
        "a valid target requests concrete backend history allocation");
  const auto first = baseline.begin_frame(true, {1920, 1080}, {960, 540},
                                         resources());
  check(first.has_value() && !first->history_frame.history_valid &&
            first->history_frame.history_slot == 0 &&
            first->history_frame.output_slot == 1,
        "first producer frame gets invalid history and the alternate output");
  check(baseline.frame_in_flight() && baseline.pending().has_value(),
        "a baseline transaction retains its exact complete input set");
  check(baseline.commit_submission() && baseline.has_submitted_resolve(),
        "only a submitted resolve enables the runtime-binding evidence");
  const auto second = baseline.begin_frame(true, {1920, 1080}, {960, 540},
                                          resources());
  check(second.has_value() && second->history_frame.history_valid &&
            second->history_frame.history_slot == 1 &&
            second->history_frame.output_slot == 0,
        "a committed output becomes the next frame's history");
  baseline.cancel_submission();
  auto missing_motion = resources();
  missing_motion.motion_vectors_written = false;
  check(!baseline.begin_frame(true, {1920, 1080}, {960, 540}, missing_motion),
        "allocated but unwritten motion vectors cannot begin a resolve");
  check(!baseline.begin_frame(false, {1920, 1080}, {960, 540}, resources()),
        "Original mode remains outside the temporal baseline");
  baseline.invalidate();
  check(!baseline.has_submitted_resolve(),
        "a discontinuity withdraws prior temporal-binding evidence");
  return failures == 0 ? 0 : 1;
}
