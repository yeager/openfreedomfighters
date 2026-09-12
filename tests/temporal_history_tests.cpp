#include "off/graphics/temporal_history.hpp"

#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
} // namespace

int main() {
  using off::graphics::TemporalHistoryLifecycle;
  using off::graphics::TemporalHistoryTarget;
  TemporalHistoryLifecycle history;
  check(!history.begin_frame(), "unconfigured history refuses a frame");
  check(!history.configure({0, 720, 1}), "zero width is rejected");
  check(!history.configure({1280, 0, 1}), "zero height is rejected");
  check(!history.configure({1280, 720, 0}), "zero format is rejected");
  check(history.configure({1280, 720, 1}), "first target requires allocation");
  check(!history.configure({1280, 720, 1}), "same target retains allocation");
  const auto first = history.begin_frame();
  check(first && first->history_slot == 0 && first->output_slot == 1 &&
            !first->history_valid,
        "first submission writes the alternate slot without history");
  check(!history.begin_frame(), "overlapping submissions are rejected");
  check(history.commit_frame(), "completed submission publishes history");
  const auto second = history.begin_frame();
  check(second && second->history_slot == 1 && second->output_slot == 0 &&
            second->history_valid,
        "next submission reads the published history slot");
  history.cancel_frame();
  const auto retry = history.begin_frame();
  check(retry && retry->history_slot == 1 && retry->history_valid,
        "cancel retains last submitted history");
  history.cancel_frame();
  history.invalidate();
  const auto cut = history.begin_frame();
  check(cut && !cut->history_valid,
        "a discontinuity suppresses stale temporal history");
  history.cancel_frame();
  check(history.configure({1920, 1080, 1}),
        "extent changes require fresh double-buffer allocation");
  const auto resized = history.begin_frame();
  check(resized && !resized->history_valid && resized->generation > first->generation,
        "reallocation invalidates history and advances generation");
  history.cancel_frame();
  return failures == 0 ? 0 : 1;
}
