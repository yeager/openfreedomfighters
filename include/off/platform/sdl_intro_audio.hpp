#pragma once
#include "off/audio/stereo_stream_playback.hpp"
#include "off/graphics/intro_prepared_resources.hpp"

namespace off::platform {
// Commands resolve only source-validated WHD links from this scene; output
// always uses real SDL devices. This factory does not admit scene
// listeners/groups, prepare records, or fake a receive pass. The created
// player retains a source-only resolver, so the prepared audio object may be
// destroyed after this call; no scene cue or lifecycle is implied.
[[nodiscard]] std::unique_ptr<audio::StereoStreamPlayback> make_sdl_intro_audio_playback(
    const graphics::IntroPreparedAudio& bank,audio::StereoStreamPlaybackLimits limits={});
} // namespace off::platform
