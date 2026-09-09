#include "off/platform/sdl_audio_output.hpp"

#include <SDL3/SDL.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace off::platform {
namespace {
constexpr std::uint32_t logical_input_rate_hz = 10000;
[[noreturn]] void fail(const char* operation) {
  throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
[[noreturn]] void unavailable(const char* operation) {
  throw SdlAudioUnavailable(std::string(operation) + ": " + SDL_GetError());
}
} // namespace

struct SdlAudioOutput::Impl {
  SDL_AudioStream* stream{};
  bool owns_audio{};
  bool submitted{};
  std::size_t limit{};
  std::string driver_name;
  ~Impl() {
    if (stream) SDL_DestroyAudioStream(stream);
    if (owns_audio) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
};

SdlAudioOutput::SdlAudioOutput(std::uint32_t sample_rate,
                             std::size_t max_queued_bytes) {
  if (!sample_rate || sample_rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    throw std::invalid_argument("SDL audio output: invalid sample rate");
  if (max_queued_bytes < 4 || max_queued_bytes % 4 != 0 ||
      max_queued_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw std::invalid_argument("SDL audio output: invalid stereo queue capacity");
  auto p = std::make_unique<Impl>();
  p->limit = max_queued_bytes;
  // SDL subsystem initialization is reference-counted, including when the host
  // has already initialized audio. Balance only our own successful reference.
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) unavailable("initialize SDL audio output");
  p->owns_audio = true;
  const char* driver = SDL_GetCurrentAudioDriver();
  if (!driver) fail("query SDL audio driver");
  p->driver_name = driver;
  if (p->driver_name == "dummy" || p->driver_name == "disk")
    throw std::runtime_error("SDL audio output requires a real playback driver; selected " + p->driver_name);
  // SDL's frequency ratio is applied while it consumes queued input. Feeding
  // the stable 10 kHz logical rate makes the legacy [100,100000] Hz command
  // range map exactly to SDL's documented [0.01,10] range. The decoded PCM is
  // still submitted unchanged; the ratio restores its requested time scale.
  const SDL_AudioSpec format{SDL_AUDIO_S16, 2, static_cast<int>(logical_input_rate_hz)};
  // Public SDL contract: opens an independent logical device, initially paused;
  // destroying this stream also closes that logical device.
  // https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream
  // https://wiki.libsdl.org/SDL3/SDL_OpenAudioDevice
  p->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &format, nullptr, nullptr);
  if (!p->stream) unavailable("open SDL stereo playback stream");
  impl_ = std::move(p);
}

SdlAudioOutput::~SdlAudioOutput() = default;

std::size_t SdlAudioOutput::queued_input_bytes() const {
  const int queued = SDL_GetAudioStreamQueued(impl_->stream);
  if (queued < 0) fail("query SDL queued input PCM");
  return static_cast<std::size_t>(queued);
}

void SdlAudioOutput::submit(std::span<const std::int16_t> samples) {
  if (samples.size() % 2 != 0)
    throw std::invalid_argument("SDL audio output: partial stereo frame");
  if (samples.empty()) return;
  const auto queued = queued_input_bytes();
  if (queued > impl_->limit || samples.size() > (impl_->limit - queued) / sizeof(std::int16_t))
    throw std::runtime_error("SDL audio output: PCM queue capacity exceeded");
  if (!SDL_PutAudioStreamData(impl_->stream, samples.data(), static_cast<int>(samples.size_bytes())))
    fail("submit SDL stereo PCM");
  impl_->submitted = true;
}

void SdlAudioOutput::start() {
  if (!impl_->submitted)
    throw std::runtime_error("SDL audio output: start requires submitted PCM");
  if (!SDL_ResumeAudioStreamDevice(impl_->stream)) fail("start SDL audio playback");
}

void SdlAudioOutput::pause() {
  if (!SDL_PauseAudioStreamDevice(impl_->stream)) fail("pause SDL audio playback");
}

void SdlAudioOutput::stop() {
  pause();
  if (!SDL_ClearAudioStream(impl_->stream)) fail("clear SDL audio playback");
  impl_->submitted = false;
}

void SdlAudioOutput::flush() {
  if (!SDL_FlushAudioStream(impl_->stream)) fail("flush SDL audio playback");
}

void SdlAudioOutput::set_gain(float linear_gain) {
  if (!std::isfinite(linear_gain) || linear_gain < 0.0F)
    throw std::invalid_argument("SDL audio output: invalid linear gain");
  if (!SDL_SetAudioStreamGain(impl_->stream, linear_gain)) fail("set SDL audio gain");
}

float SdlAudioOutput::linear_gain_from_volume(std::int32_t volume) {
  if (volume < -10000 || volume > 0)
    throw std::invalid_argument("SDL audio output: volume outside [-10000,0] hundredths dB");
  if (volume == -10000) return 0.0F;
  return static_cast<float>(std::pow(10.0, static_cast<double>(volume) / 2000.0));
}

float SdlAudioOutput::frequency_ratio_for(std::uint32_t frequency_hz) {
  if (frequency_hz < 100 || frequency_hz > 100000)
    throw std::invalid_argument("SDL audio output: frequency outside [100,100000] Hz");
  return static_cast<float>(frequency_hz) / static_cast<float>(logical_input_rate_hz);
}

void SdlAudioOutput::set_volume_hundredths_db(std::int32_t volume) {
  set_gain(linear_gain_from_volume(volume));
}

void SdlAudioOutput::set_frequency(std::uint32_t frequency_hz) {
  const float ratio = frequency_ratio_for(frequency_hz);
  if (!SDL_SetAudioStreamFrequencyRatio(impl_->stream, ratio))
    fail("set SDL audio playback frequency");
}

void SdlAudioOutput::set_pan(std::int32_t pan) {
  if (pan != 0)
    throw std::runtime_error("SDL audio output: nonzero stereo pan is unsupported");
}

const std::string& SdlAudioOutput::driver() const noexcept { return impl_->driver_name; }
} // namespace off::platform
