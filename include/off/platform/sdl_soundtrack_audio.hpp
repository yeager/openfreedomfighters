#pragma once

#include "off/audio/soundtrack_playback.hpp"

#include <memory>

namespace off::platform {

// Creates the concrete SDL output boundary for a caller-selected, verified
// soundtrack edition. Selecting an edition remains outside this factory: an
// album ordinal is not evidence of an original game cue, loop, or transition.
[[nodiscard]] std::unique_ptr<audio::SoundtrackPlayback>
make_sdl_soundtrack_playback(audio::SoundtrackPlaybackLimits limits = {});

} // namespace off::platform
