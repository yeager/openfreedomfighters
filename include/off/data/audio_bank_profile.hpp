#pragma once

#include "off/data/audio_bank_header.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <span>

namespace off::data {

// Aggregate stream metadata only. It deliberately retains neither paths nor
// audio payloads, so it is safe for diagnostics and quality comparisons.
class AudioBankProfile final {
 public:
  void add(std::span<const AudioStreamRecord> records);

  [[nodiscard]] std::size_t record_count() const noexcept { return record_count_; }
  [[nodiscard]] std::size_t pcm_record_count() const noexcept { return pcm_record_count_; }
  [[nodiscard]] std::size_t ima_adpcm_record_count() const noexcept {
    return ima_adpcm_record_count_;
  }
  [[nodiscard]] std::size_t vorbis_record_count() const noexcept {
    return vorbis_record_count_;
  }
  [[nodiscard]] std::size_t other_record_count() const noexcept {
    return other_record_count_;
  }
  [[nodiscard]] std::size_t distinct_sample_rate_count() const noexcept {
    return sample_rates_.size();
  }
  [[nodiscard]] std::size_t distinct_channel_count() const noexcept {
    return channels_.size();
  }
  [[nodiscard]] std::uint32_t maximum_sample_rate() const noexcept {
    return maximum_sample_rate_;
  }
  [[nodiscard]] std::uint32_t maximum_bits_per_sample() const noexcept {
    return maximum_bits_per_sample_;
  }
  [[nodiscard]] std::uint32_t maximum_channels() const noexcept {
    return maximum_channels_;
  }

 private:
  std::size_t record_count_{};
  std::size_t pcm_record_count_{};
  std::size_t ima_adpcm_record_count_{};
  std::size_t vorbis_record_count_{};
  std::size_t other_record_count_{};
  std::set<std::uint32_t> sample_rates_;
  std::set<std::uint32_t> channels_;
  std::uint32_t maximum_sample_rate_{};
  std::uint32_t maximum_bits_per_sample_{};
  std::uint32_t maximum_channels_{};
};

}  // namespace off::data
