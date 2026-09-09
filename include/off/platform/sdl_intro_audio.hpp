#pragma once
#include "off/audio/stereo_stream_playback.hpp"
#include "off/graphics/intro_prepared_resources.hpp"

namespace off::platform {
// The owned bank must outlive the service. Commands resolve their actual WHD
// links through that bank; output always uses real SDL devices. This factory
// does not admit scene listeners/groups, prepare records, or fake a receive pass.
[[nodiscard]] std::unique_ptr<audio::StereoStreamPlayback> make_sdl_intro_audio_playback(
    const graphics::IntroPreparedAudio& bank,audio::StereoStreamPlaybackLimits limits={});
} // namespace off::platform
