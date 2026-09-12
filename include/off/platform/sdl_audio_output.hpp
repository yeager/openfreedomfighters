#pragma once

#include "off/audio/stereo_pcm_output.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace off::platform {

// Device admission is unavailable. Only SDL subsystem initialization and device
// open failures use this type; invalid configuration and forbidden backends do
// not. Hosts/tests may explicitly report unavailable output without treating
// failures after successful admission as absence of a device.
class SdlAudioUnavailable final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct SdlAudioCapabilities {
  bool nonspatial_stereo = true;
  bool spatial_effects = false;
  bool nonzero_pan = false;
};

// A real, initially paused signed-16 stereo output. `sample_rate` is installed
// as the initial source frequency, so submitted PCM has the expected duration
// even before a later legacy frequency command. Each instance owns a separate
// SDL logical playback device, mixed by SDL with other instances. Construct and
// use on the manager thread; destroy all instances before global SDL_Quit().
// Errors throw std::runtime_error (invalid inputs: std::invalid_argument).
// No method publishes a playback acknowledgement: that is the manager's job.
class SdlAudioOutput final : public audio::StereoPcmOutput {
public:
  explicit SdlAudioOutput(std::uint32_t sample_rate,
                          std::size_t max_queued_bytes = 1024 * 1024);
  ~SdlAudioOutput() override;
  SdlAudioOutput(const SdlAudioOutput&) = delete;
  SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;

  // Copies interleaved native-endian stereo samples. Rejects partial frames and
  // submissions exceeding the bounded queue; callers refill as SDL consumes it.
  void submit(std::span<const std::int16_t> samples) override;
  [[nodiscard]] std::size_t queued_input_bytes() const override;
  // Starts/resumes only after a successful PCM submission. Success establishes
  // SDL device admission, not proof of audibility or completed hardware output.
  void start() override;
  void pause() override;
  void stop() override; // Pause and discard queued PCM.
  void flush() override; // Mark the end of submitted data, allowing resampler tail drain.
  // Native SDL amplitude adaptation: -10000 is silence, otherwise 10^(db/2000).
  // Domain is [-10000,0]; this is not original transcendental bit-exact math.
  void set_volume_hundredths_db(std::int32_t volume) override;
  [[nodiscard]] static float linear_gain_from_volume(std::int32_t volume);
  // The legacy command rate is represented through SDL's documented frequency
  // ratio range. This is pure so boundary behavior is testable without a device.
  [[nodiscard]] static float frequency_ratio_for(std::uint32_t frequency_hz);
  void set_gain(float linear_gain);
  void set_frequency(std::uint32_t frequency_hz) override;
  // Actual intro pan is zero. Other pan values are an explicit unsupported path.
  void set_pan(std::int32_t pan) override;
  [[nodiscard]] const std::string& driver() const noexcept;
  [[nodiscard]] static constexpr SdlAudioCapabilities capabilities() { return {}; }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace off::platform
