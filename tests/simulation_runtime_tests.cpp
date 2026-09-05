#include "off/simulation/fixed_step.hpp"
#include "off/simulation/input.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main() {
  using namespace off::simulation;

  InputAccumulator input;
  input.set_action(DigitalAction::fire, true);
  input.set_action(DigitalAction::fire, true);
  input.set_axis(AnalogAxis::move_x, -12'345);
  auto first = input.take_snapshot(1);
  check(first.is_held(DigitalAction::fire) &&
            first.was_pressed(DigitalAction::fire) &&
            !first.was_released(DigitalAction::fire),
        "capture a press exactly once");
  check(first.axes[static_cast<std::size_t>(AnalogAxis::move_x)] == -12'345,
        "retain quantized analog input");
  check(first.tick == 1, "bind an input snapshot to a simulation tick");
  auto second = input.take_snapshot(2);
  check(second.is_held(DigitalAction::fire) &&
            !second.was_pressed(DigitalAction::fire),
        "preserve held state without repeating edges");
  input.set_action(DigitalAction::fire, false);
  input.set_action(DigitalAction::fire, true);
  auto transitions = input.take_snapshot(3);
  check(transitions.is_held(DigitalAction::fire) &&
            transitions.was_pressed(DigitalAction::fire) &&
            transitions.was_released(DigitalAction::fire),
        "preserve both edges when a tap cycle occurs between ticks");
  input.release_all();
  const auto focus_loss = input.take_snapshot(4);
  check(!focus_loss.is_held(DigitalAction::fire) &&
            focus_loss.was_released(DigitalAction::fire) &&
            focus_loss.axes[static_cast<std::size_t>(AnalogAxis::move_x)] == 0,
        "release held input and neutralize axes on focus loss");

  FixedStepScheduler scheduler;
  std::size_t steps = 0;
  for (int frame = 0; frame < 60; ++frame)
    steps += scheduler.advance(16'666'667).step_count;
  check(steps == 60 && scheduler.completed_steps() == 60,
        "accumulate an exact rational 60 Hz schedule");
  const auto partial = scheduler.advance(8'000'000);
  check(partial.step_count == 0 && partial.interpolation_alpha > 0.47 &&
            partial.interpolation_alpha < 0.49,
        "expose presentation interpolation without advancing simulation");

  FixedStepScheduler bounded(60, 3, 100'000'000);
  const auto stalled = bounded.advance(500'000'000);
  check(stalled.step_count == 3 && stalled.elapsed_was_clamped &&
            stalled.dropped_step_count == 3 &&
            stalled.interpolation_alpha < 1.0,
        "bound stalls and discard excess backlog deterministically");
  bounded.reset();
  check(bounded.completed_steps() == 0,
        "reset accumulated simulation time and step identity");

  bool rejected = false;
  try {
    FixedStepScheduler invalid(0);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "reject invalid scheduler configuration");
}
