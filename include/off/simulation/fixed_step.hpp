#pragma once

#include <cstddef>
#include <cstdint>

namespace off::simulation {

struct StepBatch {
  std::size_t step_count{};
  std::size_t dropped_step_count{};
  double interpolation_alpha{};
  bool elapsed_was_clamped{};
};

class FixedStepScheduler final {
public:
  explicit FixedStepScheduler(std::uint32_t tick_rate = 60,
                              std::size_t maximum_steps_per_frame = 8,
                              std::uint64_t maximum_elapsed_nanoseconds =
                                  250'000'000);

  [[nodiscard]] StepBatch advance(std::uint64_t elapsed_nanoseconds) noexcept;
  void reset() noexcept;

  [[nodiscard]] std::uint32_t tick_rate() const noexcept { return tick_rate_; }
  [[nodiscard]] std::uint64_t completed_steps() const noexcept {
    return completed_steps_;
  }

private:
  static constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000;
  std::uint32_t tick_rate_;
  std::size_t maximum_steps_per_frame_;
  std::uint64_t maximum_elapsed_nanoseconds_;
  std::uint64_t accumulator_units_{};
  std::uint64_t completed_steps_{};
};

} // namespace off::simulation
