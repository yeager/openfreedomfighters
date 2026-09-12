#pragma once

#include "off/audio/soundtrack_catalog.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace off::audio {

// Opaque, locally observed game-music identifier. It carries no title,
// filename, source text, timing, or retail payload.
using SoundtrackCueToken = std::uint64_t;

// A reviewed binding must identify one exact enrolled edition. This is a
// contract for a private, evidence-backed mapping; the project ships no such
// mappings and cannot infer one from album ordinals or decoded audio.
struct ReviewedSoundtrackCueBinding {
  SoundtrackCueToken cue_token{};
  std::uint8_t album_ordinal{};
  SoundtrackFormat format{};
  std::string expected_sha256;
};

enum class SoundtrackCueSource { game_audio, optional_soundtrack };

struct SoundtrackCueResolution {
  SoundtrackCueSource source{SoundtrackCueSource::game_audio};
  const SoundtrackEdition* edition{};
};

// Resolves only an explicitly reviewed cue binding and rechecks the selected
// optional file's full SHA-256 before admitting it. Any absent, malformed,
// stale, unreadable, or mismatched entry falls back to the original game-audio
// path. This class neither decodes nor starts playback.
class SoundtrackCueResolver final {
 public:
  [[nodiscard]] static SoundtrackCueResolver from_reviewed_bindings(
      const SoundtrackCatalog& catalog,
      std::span<const ReviewedSoundtrackCueBinding> bindings);

  [[nodiscard]] SoundtrackCueResolution resolve(
      SoundtrackCueToken cue_token) const noexcept;

 private:
  struct Binding {
    SoundtrackCueToken cue_token{};
    const SoundtrackEdition* edition{};
  };

  std::vector<Binding> bindings_;
};

}  // namespace off::audio
