#include "off/audio/soundtrack_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace off::audio {
namespace {

std::string lower_extension(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extension;
}

std::optional<std::uint8_t> album_ordinal(const std::filesystem::path& path) {
  const auto name = path.stem().string();
  for (std::size_t index = 0; index + 4U <= name.size(); ++index) {
    if (name[index] != '-' || index + 4U >= name.size() ||
        name[index + 1U] != ' ' ||
        !std::isdigit(static_cast<unsigned char>(name[index + 2U])) ||
        !std::isdigit(static_cast<unsigned char>(name[index + 3U])) ||
        name[index + 4U] != ' ')
      continue;
    const auto value = static_cast<unsigned>(name[index + 2U] - '0') * 10U +
                       static_cast<unsigned>(name[index + 3U] - '0');
    if (value == 0U || value > 255U) return std::nullopt;
    return static_cast<std::uint8_t>(value);
  }
  return std::nullopt;
}

std::string album_identity(const std::filesystem::path& path) {
  const auto identity = path.stem().string();
  if (identity.empty())
    throw std::invalid_argument("verified soundtrack candidate lacks an album identity");
  return identity;
}

SoundtrackFormat format_for(const std::filesystem::path& path) {
  const auto extension = lower_extension(path);
  if (extension == ".flac") return SoundtrackFormat::flac;
  if (extension == ".mp3") return SoundtrackFormat::mp3;
  throw std::invalid_argument("verified soundtrack candidate has an unsupported format");
}

}  // namespace

SoundtrackCatalog SoundtrackCatalog::from_verified_candidates(
    std::span<const data::VerifiedSoundtrackCandidate> candidates) {
  SoundtrackCatalog catalog;
  for (const auto& candidate : candidates) {
    const auto& path = candidate.path;
    const auto ordinal = album_ordinal(path);
    if (!ordinal)
      throw std::invalid_argument("verified soundtrack candidate lacks an album ordinal");
    const auto identity = album_identity(path);
    if (candidate.expected_sha256.size() != 64U ||
        !std::all_of(candidate.expected_sha256.begin(),
                     candidate.expected_sha256.end(), [](const char value) {
                       return (value >= '0' && value <= '9') ||
                              (value >= 'a' && value <= 'f');
                     }))
      throw std::invalid_argument("verified soundtrack candidate lacks a SHA-256 identity");
    const SoundtrackEdition edition{format_for(path), path,
                                    candidate.expected_sha256};
    const auto track = std::ranges::find(catalog.tracks_, *ordinal,
                                         &SoundtrackTrack::album_ordinal);
    if (track == catalog.tracks_.end()) {
      catalog.tracks_.push_back({*ordinal, identity, edition, std::nullopt});
      continue;
    }
    auto& target = *track;
    if (target.album_identity != identity)
      throw std::invalid_argument("verified soundtrack editions have conflicting album identities");
    if (target.preferred.format == edition.format ||
        (target.fallback && target.fallback->format == edition.format))
      throw std::invalid_argument("duplicate verified soundtrack edition");
    if (edition.format == SoundtrackFormat::flac) {
      target.fallback = std::move(target.preferred);
      target.preferred = edition;
    } else {
      target.fallback = edition;
    }
  }
  std::ranges::sort(catalog.tracks_, {}, &SoundtrackTrack::album_ordinal);
  return catalog;
}

const SoundtrackTrack* SoundtrackCatalog::find_album_track(
    std::uint8_t album_ordinal) const noexcept {
  const auto found = std::ranges::find(tracks_, album_ordinal,
                                       &SoundtrackTrack::album_ordinal);
  return found == tracks_.end() ? nullptr : &*found;
}

std::span<const SoundtrackTrack> SoundtrackCatalog::tracks() const noexcept {
  return tracks_;
}

}  // namespace off::audio
