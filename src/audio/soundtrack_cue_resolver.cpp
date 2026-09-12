#include "off/audio/soundtrack_cue_resolver.hpp"

#include "off/crypto/sha256.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace off::audio {
namespace {

bool is_lower_sha256(const std::string& value) {
  return value.size() == 64U &&
         std::all_of(value.begin(), value.end(), [](const char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

bool current_file_matches(const SoundtrackEdition& edition) noexcept {
  try {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(edition.path, error);
    if (error || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status))
      return false;
    return crypto::to_hex(crypto::sha256_file(edition.path)) ==
           edition.expected_sha256;
  } catch (...) {
    return false;
  }
}

}  // namespace

SoundtrackCueResolver SoundtrackCueResolver::from_reviewed_bindings(
    const SoundtrackCatalog& catalog,
    const std::span<const ReviewedSoundtrackCueBinding> bindings) {
  SoundtrackCueResolver result;
  result.bindings_.reserve(bindings.size());
  for (const auto& binding : bindings) {
    if (!is_lower_sha256(binding.expected_sha256))
      throw std::invalid_argument("reviewed soundtrack cue binding has invalid SHA-256 identity");
    if (std::ranges::find(result.bindings_, binding.cue_token,
                          &Binding::cue_token) != result.bindings_.end())
      throw std::invalid_argument("duplicate reviewed soundtrack cue token");
    const auto* track = catalog.find_album_track(binding.album_ordinal);
    if (track == nullptr)
      throw std::invalid_argument("reviewed soundtrack cue binding has no enrolled album track");
    const auto* edition = track->preferred.format == binding.format
                              ? &track->preferred
                              : track->fallback && track->fallback->format == binding.format
                                    ? &*track->fallback
                                    : nullptr;
    if (edition == nullptr || edition->expected_sha256 != binding.expected_sha256)
      throw std::invalid_argument("reviewed soundtrack cue binding does not match enrolled edition identity");
    result.bindings_.push_back({binding.cue_token, edition});
  }
  return result;
}

SoundtrackCueResolution SoundtrackCueResolver::resolve(
    const SoundtrackCueToken cue_token) const noexcept {
  const auto found = std::ranges::find(bindings_, cue_token, &Binding::cue_token);
  if (found == bindings_.end() || found->edition == nullptr ||
      !current_file_matches(*found->edition))
    return {};
  return {.source = SoundtrackCueSource::optional_soundtrack,
          .edition = found->edition};
}

}  // namespace off::audio
