#include "off/simulation/fixed_step.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace off::simulation {

FixedStepScheduler::FixedStepScheduler(
    std::uint32_t tick_rate, std::size_t maximum_steps_per_frame,
    std::uint64_t maximum_elapsed_nanoseconds)
    : tick_rate_(tick_rate), maximum_steps_per_frame_(maximum_steps_per_frame),
      maximum_elapsed_nanoseconds_(maximum_elapsed_nanoseconds) {
  if (tick_rate == 0 || maximum_steps_per_frame == 0 ||
      maximum_elapsed_nanoseconds == 0 || tick_rate > nanoseconds_per_second ||
      maximum_elapsed_nanoseconds >
          std::numeric_limits<std::uint64_t>::max() / tick_rate) {
    throw std::invalid_argument("invalid fixed-step scheduler configuration");
  }
}

StepBatch FixedStepScheduler::advance(
    std::uint64_t elapsed_nanoseconds) noexcept {
  StepBatch result;
  if (elapsed_nanoseconds > maximum_elapsed_nanoseconds_) {
    elapsed_nanoseconds = maximum_elapsed_nanoseconds_;
    result.elapsed_was_clamped = true;
  }

  const auto maximum_add =
      std::numeric_limits<std::uint64_t>::max() - accumulator_units_;
  const auto elapsed_units = elapsed_nanoseconds * tick_rate_;
  accumulator_units_ += std::min(elapsed_units, maximum_add);
  const auto available = accumulator_units_ / nanoseconds_per_second;
  result.step_count = static_cast<std::size_t>(std::min<std::uint64_t>(
      available, static_cast<std::uint64_t>(maximum_steps_per_frame_)));
  accumulator_units_ -=
      static_cast<std::uint64_t>(result.step_count) * nanoseconds_per_second;
  completed_steps_ += std::min<std::uint64_t>(
      result.step_count,
      std::numeric_limits<std::uint64_t>::max() - completed_steps_);

  if (available > maximum_steps_per_frame_) {
    result.dropped_step_count = static_cast<std::size_t>(
        available - static_cast<std::uint64_t>(maximum_steps_per_frame_));
    accumulator_units_ %= nanoseconds_per_second;
  }
  result.interpolation_alpha = static_cast<double>(accumulator_units_) /
                               static_cast<double>(nanoseconds_per_second);
  return result;
}

void FixedStepScheduler::reset() noexcept {
  accumulator_units_ = 0;
  completed_steps_ = 0;
}

} // namespace off::simulation
