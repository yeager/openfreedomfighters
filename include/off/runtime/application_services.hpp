#pragma once
#include "off/audio/sound_preferences.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/runtime/application_clock.hpp"
#include <utility>

namespace off::runtime {
// Application-lifetime state, borrowed by scenes rather than reset when each
// IntroRuntime is constructed. No global numeric/scene-reference stores are
// inferred from this separate text configuration or from backend absence.
class ApplicationServices final {
public:
  ApplicationServices(ClockExecutionPolicy policy, ClockSamplingServices sampling,
                      std::function<audio::SoundVolumeBackend*()> live_backend)
    : sampling_(std::move(sampling)), clock_(policy),
      sound_({configuration_,std::move(live_backend)}) {}
  [[nodiscard]] ApplicationClock& clock() noexcept { return clock_; }
  [[nodiscard]] const ApplicationClock& clock() const noexcept { return clock_; }
  [[nodiscard]] audio::SoundPreferences& sound() noexcept { return sound_; }
  [[nodiscard]] audio::SoundTextConfiguration& configuration() noexcept { return configuration_; }
  void reset_clock() { clock_.reset(sampling_); }
  void rebase_clock() { clock_.rebase(sampling_); }
  std::int32_t advance_crt() { return clock_.advance_crt(sampling_); }
  // Leaves actual input, global numeric property and renderer services intact.
  // Binding is transactional; this owner must outlive all resulting callbacks.
  void bind_controller_phase_two(graphics::IntroControllerPhaseTwoServices& supplied) {
    auto bound=supplied;
    sound_.bind_phase_two_services(bound);
    bound.assign_engine_clock_mode=[this](bool value) { clock_.assign_crt_mode(value); };
    bound.scene_integer_clock=[this] { return clock_.scene_integer_word(); };
    supplied=std::move(bound);
  }
private:
  ClockSamplingServices sampling_;
  ApplicationClock clock_;
  audio::SoundTextConfiguration configuration_;
  audio::SoundPreferences sound_;
};
} // namespace off::runtime
