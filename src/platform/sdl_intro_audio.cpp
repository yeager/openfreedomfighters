#include "off/platform/sdl_intro_audio.hpp"
#include "off/platform/sdl_audio_output.hpp"

namespace off::platform {
std::unique_ptr<audio::StereoStreamPlayback> make_sdl_intro_audio_playback(
    const graphics::IntroPreparedAudio& bank,audio::StereoStreamPlaybackLimits limits) {
  return std::make_unique<audio::StereoStreamPlayback>(
    [&bank](std::uint32_t link) {return bank.open_source_for_sound_link(link);},
    [](std::uint32_t rate,std::size_t bytes) {
      return std::make_unique<SdlAudioOutput>(rate,bytes);
    },limits);
}
} // namespace off::platform
