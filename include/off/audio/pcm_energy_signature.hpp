#pragma once

#include "off/audio/decode.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace off::audio {

// Content-derived comparison input. It contains no source identity, filename,
// decoded PCM, or cue semantics. A similarity score is evidence only and must
// never select playback without an independently verified cue contract.
struct PcmEnergySignature final {
  std::vector<float> bins;
};

[[nodiscard]] inline PcmEnergySignature make_pcm_energy_signature(
    const DecodedAudio& audio, std::uint32_t bins_per_second = 100U) {
  if (audio.sample_rate == 0U || (audio.channels != 1U && audio.channels != 2U) ||
      audio.frame_count() == 0U || bins_per_second == 0U ||
      bins_per_second > audio.sample_rate) {
    throw std::invalid_argument("PCM energy signature has invalid audio metadata");
  }
  const auto frames = static_cast<std::uint64_t>(audio.frame_count());
  if (frames > std::numeric_limits<std::uint64_t>::max() / bins_per_second) {
    throw std::invalid_argument("PCM energy signature duration overflows");
  }
  const auto count = (frames * bins_per_second + audio.sample_rate - 1U) /
                     audio.sample_rate;
  if (count == 0U || count > 1'000'000U) {
    throw std::invalid_argument("PCM energy signature exceeds its bin limit");
  }
  PcmEnergySignature result;
  result.bins.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0U; index < count; ++index) {
    const auto first = index * audio.sample_rate / bins_per_second;
    const auto last = std::min(frames, (index + 1U) * audio.sample_rate /
                                          bins_per_second);
    if (last <= first) throw std::runtime_error("PCM energy signature has an empty bin");
    long double energy{};
    for (std::uint64_t frame = first; frame < last; ++frame) {
      long double mono{};
      for (std::uint32_t channel = 0U; channel < audio.channels; ++channel) {
        mono += audio.interleaved_samples[frame * audio.channels + channel];
      }
      mono /= audio.channels;
      energy += mono * mono;
    }
    const auto rms = std::sqrt(energy / static_cast<long double>(last - first));
    result.bins.push_back(static_cast<float>(std::log1pl(rms / 32'768.0L)));
  }
  return result;
}

[[nodiscard]] inline float pcm_energy_similarity(
    std::span<const float> first, std::span<const float> second) {
  if (first.empty() || first.size() != second.size()) {
    throw std::invalid_argument("PCM energy comparison requires equal nonempty signatures");
  }
  long double first_mean{}, second_mean{};
  for (std::size_t index = 0; index < first.size(); ++index) {
    if (!std::isfinite(first[index]) || !std::isfinite(second[index])) {
      throw std::invalid_argument("PCM energy comparison has non-finite input");
    }
    first_mean += first[index];
    second_mean += second[index];
  }
  first_mean /= first.size();
  second_mean /= second.size();
  long double numerator{}, first_energy{}, second_energy{};
  for (std::size_t index = 0; index < first.size(); ++index) {
    const auto left = static_cast<long double>(first[index]) - first_mean;
    const auto right = static_cast<long double>(second[index]) - second_mean;
    numerator += left * right;
    first_energy += left * left;
    second_energy += right * right;
  }
  if (first_energy == 0.0L || second_energy == 0.0L) {
    throw std::invalid_argument("PCM energy comparison has no dynamic range");
  }
  return static_cast<float>(numerator / std::sqrt(first_energy * second_energy));
}

}  // namespace off::audio
