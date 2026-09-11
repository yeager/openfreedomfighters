#include "off/data/audio_bank_profile.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool value, const char* message) {
  if (!value) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  using off::data::AudioBankProfile;
  using off::data::AudioStreamRecord;
  const std::array records{
      AudioStreamRecord{.format_flags = 1U, .sample_rate = 22'050U,
                        .bits_per_sample = 16U, .channels = 1U},
      AudioStreamRecord{.format_flags = 0x11U, .sample_rate = 44'100U,
                        .bits_per_sample = 4U, .channels = 2U},
      AudioStreamRecord{.format_flags = 0x80001000U, .sample_rate = 48'000U,
                        .bits_per_sample = 16U, .channels = 2U},
      AudioStreamRecord{.format_flags = 99U, .sample_rate = 8'000U,
                        .bits_per_sample = 8U, .channels = 1U},
  };
  AudioBankProfile profile;
  profile.add({records.data(), 2U});
  profile.add({records.data() + 2U, 2U});
  check(profile.record_count() == 4U, "counts every record");
  check(profile.pcm_record_count() == 1U &&
            profile.ima_adpcm_record_count() == 1U &&
            profile.vorbis_record_count() == 1U &&
            profile.other_record_count() == 1U,
        "normalizes global-bank flag before codec categorization");
  check(profile.distinct_sample_rate_count() == 4U &&
            profile.distinct_channel_count() == 2U,
        "retains only distinct metadata cardinality");
  check(profile.maximum_sample_rate() == 48'000U &&
            profile.maximum_bits_per_sample() == 16U &&
            profile.maximum_channels() == 2U,
        "reports maximum stream metadata");
}
