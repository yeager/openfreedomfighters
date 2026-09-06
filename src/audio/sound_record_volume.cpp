#include "off/audio/sound_records.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace off::audio {
namespace {
float rounded(float value) { volatile float stored = value; return stored; }

// Independently reconstructed integer response. These decibel knots reproduce
// the observed rounded response; this is not an extracted legacy lookup table.
float response(std::int32_t volume) {
  if (volume < 0 || volume > 100) return static_cast<float>(volume);
  constexpr std::array<float,4> low{0.0F, 0.18F, 0.22F, 0.26F};
  if (volume < 4) return low[static_cast<std::size_t>(volume)];
  constexpr std::array<int,9> inputs{4,8,12,16,20,28,36,44,100};
  constexpr std::array<int,9> decibels{-50,-48,-42,-38,-32,-24,-18,-14,0};
  std::size_t upper = 1;
  while (volume > inputs[upper]) ++upper;
  const double fraction = double(volume-inputs[upper-1]) / double(inputs[upper]-inputs[upper-1]);
  const double db = decibels[upper-1] + fraction * (decibels[upper]-decibels[upper-1]);
  return static_cast<float>(std::round(10000.0 * std::pow(10.0,db/20.0)) / 100.0);
}
} // namespace

void SoundRecordRegistry::request_category_volume(std::uint32_t category,
    std::int32_t volume, std::uint32_t mode) {
  if (mode != 2 || category >= categories_.size())
    throw std::runtime_error("Unsupported sound category volume mode or category");
  float gain = rounded(response(volume) * 0.01F);
  constexpr std::array<float,3> multipliers{0.56F,0.49F,0.89F};
  if (category < multipliers.size()) gain = rounded(gain * multipliers[category]);
  auto& entry = categories_[category];
  entry.gain = gain;
  if (gain > 0) {
    if (!entry.selected && !special_mode_) {
      for (const auto binding : prepared_) {
        if (auto* record = resolve(binding); record && record->category == category)
          record->playback_state = 5;
      }
    }
    entry.selected = true;
  } else {
    entry.selected = false;
  }
  pending_volume_update_ = true;
}
} // namespace off::audio
