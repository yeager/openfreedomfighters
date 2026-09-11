#include "off/audio/pcm_energy_signature.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
off::audio::DecodedAudio signal(std::int16_t scale) {
  off::audio::DecodedAudio result{.encoding = off::audio::Encoding::pcm_s16le,
                                  .sample_rate = 100U, .channels = 1U};
  for (int index = 0; index < 400; ++index) {
    const int phase = index % 100;
    const int amplitude = phase < 50 ? phase * 500 : (100 - phase) * 500;
    result.interleaved_samples.push_back(static_cast<std::int16_t>(
        amplitude * scale / 100));
  }
  return result;
}
}  // namespace
int main() {
  const auto source = off::audio::make_pcm_energy_signature(signal(100), 10U);
  const auto mastered = off::audio::make_pcm_energy_signature(signal(60), 10U);
  check(source.bins.size() == 40U, "signature uses a stable time grid");
  check(off::audio::pcm_energy_similarity(source.bins, mastered.bins) > 0.999F,
        "energy similarity ignores uniform mastering gain");
  bool rejected = false;
  try {
    static_cast<void>(off::audio::pcm_energy_similarity(
        source.bins, std::span(source.bins).subspan(1)));
  }
  catch (const std::invalid_argument&) { rejected = true; }
  check(rejected, "energy similarity rejects unequal signatures");
  std::cout << "PCM energy signature tests passed\n";
}
