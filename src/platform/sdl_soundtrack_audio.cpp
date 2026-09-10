#include "off/platform/sdl_soundtrack_audio.hpp"

#include "off/platform/sdl_audio_output.hpp"

namespace off::platform {

std::unique_ptr<audio::SoundtrackPlayback>
make_sdl_soundtrack_playback(audio::SoundtrackPlaybackLimits limits) {
  return std::make_unique<audio::SoundtrackPlayback>(
      [](std::uint32_t rate, std::size_t bytes) {
        return std::make_unique<SdlAudioOutput>(rate, bytes);
      },
      limits);
}

} // namespace off::platform
