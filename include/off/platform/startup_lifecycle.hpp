#pragma once

#include <chrono>

namespace off::platform {

using StartupClock = std::chrono::steady_clock;
inline constexpr auto startup_splash_duration = std::chrono::seconds{3};

enum class StartupPhase {
  awaiting_first_presentation,
  splash,
  loading,
  ready,
  cancelled,
};

// Pure startup timing policy. The splash deadline is anchored to the first
// successful presentation, rather than window creation or asset decoding.
class StartupLifecycle {
public:
  [[nodiscard]] StartupPhase phase() const noexcept { return phase_; }

  void presented(StartupClock::time_point now) noexcept;
  [[nodiscard]] StartupPhase tick(StartupClock::time_point now,
                                  bool preparation_ready) noexcept;
  void cancel() noexcept { phase_ = StartupPhase::cancelled; }

private:
  StartupPhase phase_{StartupPhase::awaiting_first_presentation};
  StartupClock::time_point deadline_{};
};

} // namespace off::platform
