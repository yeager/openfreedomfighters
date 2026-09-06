#pragma once
#include <cstdint>
#include <functional>
#include <optional>

namespace off::runtime {

enum class ClockExecutionPolicy { no_recording_or_replay };
struct ClockSamplingServices {
  // Successful sample boundary only. Counter failure/fallback selection belongs
  // to the platform service; an unavailable sample must throw, not invent zero.
  std::function<std::int64_t()> alternate_counter;
  std::function<std::int32_t()> crt_milliseconds;
};
// Portable native policy: elapsed steady-clock milliseconds, not POSIX clock().
// A shared origin is retained by both samplers; CRT-domain exhaustion rejects.
[[nodiscard]] ClockSamplingServices make_monotonic_clock_samples();
struct ApplicationClockState {
  float rate{1};
  bool crt_mode{}, suppress_next{};
  double unscaled_elapsed{}, prior_crt_sample{}, scaled_elapsed{}, smoothing{};
  std::int32_t raw_integer{}, previous_raw_integer{}, scaled_integer{};
  std::int32_t scene_integer{}, previous_scene_integer{}, scene_offset{};
  float raw_delta{}, last_scaled_increment{}, scene_delta{};
  std::int64_t alternate_counter{};
};
struct SceneFreezeState {
  bool current, requested;
  std::int32_t snapshot;
};

// Canonical sampled application time, separate from the native splash clock.
// Explicit no-replay, successful-counter boundary. Alternate-mode advancement
// is unsupported, but its counter is still sampled during CRT updates/rebase.
// Negative/backwards CRT samples, nonfinite arithmetic and integer conversion
// overflow reject; no POSIX clock(), saturation, or silent wrap of CRT samples.
class ApplicationClock final {
public:
  explicit ApplicationClock(ClockExecutionPolicy policy, ApplicationClockState state = {});
  ApplicationClock(const ApplicationClock&) = delete;
  ApplicationClock& operator=(const ApplicationClock&) = delete;
  [[nodiscard]] const ApplicationClockState& state() const noexcept { return state_; }
  [[nodiscard]] bool ready() const noexcept { return ready_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  void assign_crt_mode(bool mode);
  void set_rate(float rate);
  void suppress_next_accumulation();
  // Reset preserves last increment/delta, rate, mode and suppression. Freeze
  // state is external and untouched. The loader separately clears its snapshot.
  void reset(const ClockSamplingServices& sampling);
  void rebase(const ClockSamplingServices& sampling);
  std::int32_t advance_crt(const ClockSamplingServices& sampling);
  // Call after producer/replay handling, reading the live scene freeze flag then.
  void publish_scene(bool frozen);
  void apply_freeze_request(SceneFreezeState& freeze, bool requested,
                            std::int32_t sampled_upstream_integer);
  [[nodiscard]] std::uint32_t scene_integer_word() const;
private:
  void check_live() const;
  void sample_rebase(const ClockSamplingServices& sampling);
  double sample_crt(const ClockSamplingServices& sampling);
  ApplicationClockState state_;
  std::optional<std::int32_t> last_crt_input_;
  bool ready_{}, running_{}, failed_{};
};
} // namespace off::runtime
