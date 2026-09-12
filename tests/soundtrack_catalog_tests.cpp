#include "off/audio/soundtrack_catalog.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using off::audio::SoundtrackCatalog;
using off::audio::SoundtrackFormat;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <typename Function>
void rejects(Function&& operation, const char* message) {
  try {
    operation();
  } catch (const std::invalid_argument&) {
    return;
  }
  check(false, message);
}

std::filesystem::path path(const char* value) { return value; }
}  // namespace

int main() {
  const std::vector candidates{
      path("album/Artist - Game - 02 Second.mp3"),
      path("album/Artist - Game - 01 First.mp3"),
      path("album/Artist - Game - 01 First.flac"),
      path("album/Artist - Game - 02 Second.flac"),
  };
  const auto catalog = SoundtrackCatalog::from_verified_candidates(candidates);
  check(catalog.tracks().size() == 2U, "both album tracks are cataloged");
  const auto* first = catalog.find_album_track(1U);
  const auto* second = catalog.find_album_track(2U);
  check(first && second && !catalog.find_album_track(3U), "ordinal lookup is exact");
  check(first->album_identity == "Artist - Game - 01 First" &&
            second->album_identity == "Artist - Game - 02 Second",
        "catalog retains an exact album identity for each edition pair");
  check(first->preferred.format == SoundtrackFormat::flac &&
            first->fallback && first->fallback->format == SoundtrackFormat::mp3,
        "FLAC is preferred with MP3 fallback");
  check(second->preferred.format == SoundtrackFormat::flac &&
            second->fallback && second->fallback->format == SoundtrackFormat::mp3,
        "input order does not affect edition priority");
  rejects([] { static_cast<void>(SoundtrackCatalog::from_verified_candidates(
              std::vector{path("album/no-ordinal.flac")})); },
          "missing ordinal is rejected");
  rejects([] { static_cast<void>(SoundtrackCatalog::from_verified_candidates(
              std::vector{path("album/A - Game - 01 One.flac"),
                          path("album/B - Game - 01 Another.flac")})); },
          "ambiguous duplicate edition is rejected");
  rejects([] { static_cast<void>(SoundtrackCatalog::from_verified_candidates(
              std::vector{path("album/A - Game - 01 One.flac"),
                          path("album/B - Game - 01 Another.mp3")})); },
          "different titles cannot be paired only by ordinal");
  rejects([] { static_cast<void>(SoundtrackCatalog::from_verified_candidates(
              std::vector{path("album/A - Game - 01 One.ogg")})); },
          "unsupported format is rejected");
  std::cout << "soundtrack catalog tests passed\n";
}
