#include "off/audio/soundtrack_cue_resolver.hpp"
#include "off/crypto/sha256.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using off::audio::ReviewedSoundtrackCueBinding;
using off::audio::SoundtrackCatalog;
using off::audio::SoundtrackCueResolver;
using off::audio::SoundtrackCueSource;
using off::audio::SoundtrackFormat;
using off::data::VerifiedSoundtrackCandidate;
void check(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
template <class F> void rejects(F&& action, const char* message) {
  try { action(); } catch (const std::invalid_argument&) { return; }
  check(false, message);
}
class Fixture final {
 public:
  Fixture() : path(std::filesystem::current_path() /
                   "OFF - Soundtrack - 01 Cue-Resolver-Fixture.flac") {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "cue admission fixture";
    output.close();
    digest = off::crypto::to_hex(off::crypto::sha256_file(path));
  }
  ~Fixture() { std::error_code error; std::filesystem::remove(path, error); }
  std::filesystem::path path;
  std::string digest;
};
}  // namespace

int main() {
  Fixture fixture;
  const auto catalog = SoundtrackCatalog::from_verified_candidates(
      std::vector{VerifiedSoundtrackCandidate{fixture.path, fixture.digest}});
  const std::vector bindings{ReviewedSoundtrackCueBinding{
      .cue_token = 0x34U, .album_ordinal = 1U,
      .format = SoundtrackFormat::flac, .expected_sha256 = fixture.digest}};
  const auto resolver = SoundtrackCueResolver::from_reviewed_bindings(catalog, bindings);
  const auto admitted = resolver.resolve(0x34U);
  check(admitted.source == SoundtrackCueSource::optional_soundtrack &&
            admitted.edition != nullptr && admitted.edition->path == fixture.path,
        "exact reviewed file identity admits optional edition");
  const auto missing = resolver.resolve(0x35U);
  check(missing.source == SoundtrackCueSource::game_audio && missing.edition == nullptr,
        "unmapped token retains game audio");
  { std::ofstream output(fixture.path, std::ios::binary | std::ios::trunc); output << "changed"; }
  const auto changed = resolver.resolve(0x34U);
  check(changed.source == SoundtrackCueSource::game_audio && changed.edition == nullptr,
        "changed optional file retains game audio");
  rejects([&] { static_cast<void>(SoundtrackCueResolver::from_reviewed_bindings(
      catalog, std::vector{ReviewedSoundtrackCueBinding{
          .cue_token = 1U, .album_ordinal = 1U, .format = SoundtrackFormat::flac,
          .expected_sha256 = std::string(64U, 'b')}})); },
      "mismatched digest cannot bind an enrolled edition");
  rejects([&] { static_cast<void>(SoundtrackCueResolver::from_reviewed_bindings(
      catalog, std::vector{bindings.front(), bindings.front()})); },
      "duplicate opaque cue token is rejected");
  std::cout << "soundtrack cue resolver tests passed\n";
}
