#include "off/graphics/intro_audio_cue_inventory.hpp"

#include <cmath>
#include <set>
#include <stdexcept>

namespace off::graphics {

IntroAudioCueInventory IntroAudioCueInventory::from_prepared(
    const std::span<const IntroPreparedSound> sounds,
    const data::AudioBankHeader& header,
    const std::span<const std::optional<std::size_t>> record_indices) {
  if (sounds.empty() || sounds.size() != record_indices.size())
    throw std::invalid_argument("intro audio inventory has no complete prepared owner set");

  IntroAudioCueInventory result{.source_owner_count = sounds.size()};
  std::set<std::size_t> owners;
  std::set<std::uint32_t> definitions;
  std::set<std::size_t> streams;
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    const auto& sound = sounds[index];
    if (!owners.insert(sound.directory_index).second ||
        sound.source.sound_definition_reference == 0U ||
        sound.definition.definition_offset == 0U ||
        sound.definition.definition_offset != sound.source.sound_definition_reference ||
        sound.definition.resource_link == 0U ||
        !std::isfinite(sound.definition.duration) || sound.definition.duration < 0.0F)
      throw std::invalid_argument("intro audio inventory has an invalid sound-owner relation");
    const auto resolved = header.record_index_for_sound_link(sound.definition.resource_link);
    if (!resolved || !record_indices[index] || *resolved != *record_indices[index] ||
        *resolved >= header.records().size())
      throw std::invalid_argument("intro audio inventory has an unpaired SND/WHD relation");
    const auto& stream = header.records()[*resolved];
    if (stream.sample_rate == 0U || stream.bits_per_sample == 0U ||
        stream.channels == 0U || stream.encoded_size == 0U ||
        stream.data_offset == 0U || stream.sample_value_count == 0U ||
        stream.block_align == 0U || stream.samples_per_block == 0U)
      throw std::invalid_argument("intro audio inventory has incomplete WHD stream metadata");
    definitions.insert(sound.definition.definition_offset);
    if (streams.insert(*resolved).second) {
      if (stream.uses_global_bank()) ++result.global_bank_stream_count;
      else ++result.local_bank_stream_count;
    }
  }
  result.distinct_sound_definition_count = definitions.size();
  result.distinct_stream_count = streams.size();
  return result;
}

}  // namespace off::graphics
