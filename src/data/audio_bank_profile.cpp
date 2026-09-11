#include "off/data/audio_bank_profile.hpp"

#include <algorithm>

namespace off::data {

void AudioBankProfile::add(std::span<const AudioStreamRecord> records) {
  for (const auto& record : records) {
    ++record_count_;
    switch (record.format_flags & 0x7fffffffU) {
      case 1U:
        ++pcm_record_count_;
        break;
      case 0x11U:
        ++ima_adpcm_record_count_;
        break;
      case 0x1000U:
        ++vorbis_record_count_;
        break;
      default:
        ++other_record_count_;
        break;
    }
    sample_rates_.insert(record.sample_rate);
    channels_.insert(record.channels);
    maximum_sample_rate_ = std::max(maximum_sample_rate_, record.sample_rate);
    maximum_bits_per_sample_ =
        std::max(maximum_bits_per_sample_, record.bits_per_sample);
    maximum_channels_ = std::max(maximum_channels_, record.channels);
  }
}

}  // namespace off::data
