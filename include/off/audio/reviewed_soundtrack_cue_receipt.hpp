#pragma once

#include "off/audio/soundtrack_cue_resolver.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace off::audio {

// A private receipt emitted by import_reviewed_soundtrack_cue_bindings.py.
// It retains only opaque cue-to-edition identities: no filename, title,
// source text, samples, timing data, installation path, or playback state.
class ReviewedSoundtrackCueReceipt final {
 public:
  [[nodiscard]] static std::optional<ReviewedSoundtrackCueReceipt> load_local(
      const std::filesystem::path& local_directory);

  [[nodiscard]] std::span<const ReviewedSoundtrackCueBinding> bindings() const noexcept {
    return bindings_;
  }

 private:
  explicit ReviewedSoundtrackCueReceipt(std::vector<ReviewedSoundtrackCueBinding> bindings)
      : bindings_(std::move(bindings)) {}

  std::vector<ReviewedSoundtrackCueBinding> bindings_;
};

}  // namespace off::audio
