#pragma once
#include "off/audio/sound_preferences.hpp"
#include "off/audio/sound_records.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/graphics/preview_camera_update.hpp"
#include "off/runtime/application_clock.hpp"
#include "off/runtime/live_variables.hpp"
#include <utility>
#include <stdexcept>
#include <limits>

namespace off::runtime {
// Application-lifetime state, borrowed by scenes rather than reset when each
// IntroRuntime is constructed. No global numeric/scene-reference stores are
// inferred from this separate text configuration or from backend absence.
class ApplicationServices final {
public:
  ApplicationServices(ClockExecutionPolicy policy, ClockSamplingServices sampling,
                      std::function<audio::SoundVolumeBackend*()> live_backend = {})
    : sampling_(std::move(sampling)), clock_(policy),
      sound_({configuration_,live_backend ? std::move(live_backend) :
          std::function<audio::SoundVolumeBackend*()>{[this] { return &sound_records_; }}}) {}
  [[nodiscard]] ApplicationClock& clock() noexcept { return clock_; }
  [[nodiscard]] const ApplicationClock& clock() const noexcept { return clock_; }
  [[nodiscard]] audio::SoundPreferences& sound() noexcept { return sound_; }
  [[nodiscard]] graphics::PreviewCameraUpdate& preview_camera_update() noexcept { return preview_camera_update_; }
  [[nodiscard]] LiveVariableRegistry& live_variables() noexcept { return live_variables_; }
  // Native application-scoped owner IDs never alias a previously destroyed
  // scene. This is not the original allocator or the component serial counter.
  [[nodiscard]] std::uint64_t allocate_runtime_owners(std::uint64_t count) {
    if(!count || !next_runtime_owner_ || count-1>std::numeric_limits<std::uint64_t>::max()-next_runtime_owner_)
      throw std::runtime_error("Native runtime owner identity domain exhausted");
    const auto first=next_runtime_owner_;
    next_runtime_owner_+=count;
    return first;
  }
  // Explicit component-update boundary. Input queries and the live camera/queue
  // come from the caller; pointer raw delta and keyboard last scaled increment
  // always come from this canonical clock (not frozen scene publication).
  void update_preview_camera(graphics::FreshIntroCamera& owner,graphics::PreviewCameraPose& camera,
      graphics::PreviewCameraInput input,
      const std::function<void(graphics::PreviewCameraPose&)>& enqueue_transform) {
    if (!clock_.ready() || clock_.failed() || !clock_.state().crt_mode)
      throw std::runtime_error("preview camera requires the live CRT application clock");
    input.raw_crt_delta=clock_.state().raw_delta;
    input.last_scaled_increment=clock_.state().last_scaled_increment;
    preview_camera_update_.run(owner,camera,input,enqueue_transform);
  }
  // Canonical logical records survive across scene loads. This store alone is
  // not an output device or a producer of playback-start acknowledgements.
  [[nodiscard]] audio::SoundRecordRegistry& sound_records() noexcept { return sound_records_; }
  [[nodiscard]] const audio::SoundRecordRegistry& sound_records() const noexcept { return sound_records_; }
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
  audio::SoundRecordRegistry sound_records_;
  audio::SoundPreferences sound_;
  graphics::PreviewCameraUpdate preview_camera_update_;
  LiveVariableRegistry live_variables_;
  std::uint64_t next_runtime_owner_{1};
};
} // namespace off::runtime
