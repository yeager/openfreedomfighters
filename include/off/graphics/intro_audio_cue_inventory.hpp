#pragma once

#include "off/data/audio_bank_header.hpp"
#include "off/graphics/intro_prepared_resources.hpp"

#include <cstddef>
#include <span>

namespace off::graphics {

// Source-backed inventory for the authored intro sound owners.  It deliberately
// has no cue token, filename, logical identifier, timing, decoder, or playback
// API: a SND/WHD resource relation is not evidence that an optional album track
// can replace an in-game cue.
struct IntroAudioCueInventory {
  std::size_t source_owner_count{};
  std::size_t distinct_sound_definition_count{};
  std::size_t distinct_stream_count{};
  std::size_t local_bank_stream_count{};
  std::size_t global_bank_stream_count{};

  [[nodiscard]] static IntroAudioCueInventory from_prepared(
      std::span<const IntroPreparedSound> sounds,
      const data::AudioBankHeader& header,
      std::span<const std::optional<std::size_t>> record_indices);
};

}  // namespace off::graphics
