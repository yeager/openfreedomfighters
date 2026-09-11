#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace off::audio {

// Timing is evidence for further comparison only. It cannot identify a game
// cue, authorize replacement playback, or establish loop behavior.
struct AudioDuration final {
  std::uint64_t frames{};
  std::uint32_t sample_rate{};
};

struct DurationComparisonSummary final {
  std::size_t compared_pairs{};
  std::size_t exact_duration_pairs{};
  std::size_t near_duration_pairs{};
};

[[nodiscard]] inline DurationComparisonSummary compare_audio_durations(
    std::span<const AudioDuration> game_streams,
    std::span<const AudioDuration> album_tracks,
    std::uint64_t near_tolerance_microseconds) {
  if (near_tolerance_microseconds == 0U) {
    throw std::invalid_argument("audio duration comparison requires a positive tolerance");
  }
  DurationComparisonSummary result;
  for (const auto game : game_streams) {
    if (game.frames == 0U || game.sample_rate == 0U) {
      throw std::invalid_argument("game audio duration is invalid");
    }
    for (const auto album : album_tracks) {
      if (album.frames == 0U || album.sample_rate == 0U) {
        throw std::invalid_argument("album audio duration is invalid");
      }
      if (game.frames > std::numeric_limits<std::uint64_t>::max() /
                            album.sample_rate ||
          album.frames > std::numeric_limits<std::uint64_t>::max() /
                             game.sample_rate) {
        throw std::invalid_argument("audio duration comparison overflows");
      }
      ++result.compared_pairs;
      const auto left = game.frames * static_cast<std::uint64_t>(album.sample_rate);
      const auto right = album.frames * static_cast<std::uint64_t>(game.sample_rate);
      if (left == right) {
        ++result.exact_duration_pairs;
        ++result.near_duration_pairs;
        continue;
      }
      const auto delta = left > right ? left - right : right - left;
      const auto denominator = static_cast<std::uint64_t>(game.sample_rate) *
                               static_cast<std::uint64_t>(album.sample_rate);
      if (near_tolerance_microseconds >
          std::numeric_limits<std::uint64_t>::max() / denominator) {
        throw std::invalid_argument("audio duration tolerance overflows");
      }
      const auto allowed_delta =
          near_tolerance_microseconds * denominator / 1'000'000U;
      if (delta <= allowed_delta) {
        ++result.near_duration_pairs;
      }
    }
  }
  return result;
}

}  // namespace off::audio
