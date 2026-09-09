#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace off::audio {

// A catalog of hash-verified optional album files. Album ordinals identify
// editions of an album track only; they do not identify game music cues.
enum class SoundtrackFormat { flac, mp3 };

struct SoundtrackEdition {
  SoundtrackFormat format{};
  std::filesystem::path path;
};

struct SoundtrackTrack {
  std::uint8_t album_ordinal{};
  SoundtrackEdition preferred;
  std::optional<SoundtrackEdition> fallback;
};

class SoundtrackCatalog final {
 public:
  // Input must be InstallVerification::soundtrack_candidates. This does not
  // open, decode, map, or play the files; it only groups their album editions.
  [[nodiscard]] static SoundtrackCatalog from_verified_candidates(
      std::span<const std::filesystem::path> candidates);

  [[nodiscard]] const SoundtrackTrack* find_album_track(
      std::uint8_t album_ordinal) const noexcept;
  [[nodiscard]] std::span<const SoundtrackTrack> tracks() const noexcept;

 private:
  std::vector<SoundtrackTrack> tracks_;
};

}  // namespace off::audio
