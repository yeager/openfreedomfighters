#include "off/platform/startup_lifecycle.hpp"

namespace off::platform {

void StartupLifecycle::presented(StartupClock::time_point now) noexcept {
  if (phase_ != StartupPhase::awaiting_first_presentation)
    return;
  deadline_ = now > StartupClock::time_point::max() - startup_splash_duration
                  ? StartupClock::time_point::max()
                  : now + startup_splash_duration;
  phase_ = StartupPhase::splash;
}

StartupPhase StartupLifecycle::tick(StartupClock::time_point now,
                                    bool preparation_ready) noexcept {
  if (phase_ == StartupPhase::splash && now >= deadline_)
    phase_ = preparation_ready ? StartupPhase::ready : StartupPhase::loading;
  else if (phase_ == StartupPhase::loading && preparation_ready)
    phase_ = StartupPhase::ready;
  return phase_;
}

} // namespace off::platform
