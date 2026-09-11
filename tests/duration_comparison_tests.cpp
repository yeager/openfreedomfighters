#include "off/audio/duration_comparison.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  constexpr std::array<off::audio::AudioDuration, 2> game{{
      {44'100U, 44'100U}, {88'200U, 44'100U}}};
  constexpr std::array<off::audio::AudioDuration, 2> album{{
      {22'050U, 22'050U}, {52'920U, 44'100U}}};
  const auto comparison = off::audio::compare_audio_durations(
      game, album, 250'000U);
  check(comparison.compared_pairs == 4U,
        "duration comparison examines every cross-source pair");
  check(comparison.exact_duration_pairs == 1U,
        "duration comparison retains rate-independent exact durations");
  check(comparison.near_duration_pairs == 2U,
        "duration comparison includes only pairs within the explicit tolerance");

  bool rejected = false;
  try {
    static_cast<void>(off::audio::compare_audio_durations(game, album, 0U));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected, "duration comparison rejects an absent tolerance");
  std::cout << "duration comparison tests passed\n";
}
