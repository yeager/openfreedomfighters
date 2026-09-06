#include "off/data/install_manifest.hpp"
#include "off/crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace off::data;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
void write(const std::filesystem::path& path, std::string_view bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output) throw std::runtime_error("could not write independent manifest fixture");
}
bool has(const ManifestVerification& result, std::string_view path, ManifestFileStatus status) {
  return std::any_of(result.files.begin(), result.files.end(), [&](const auto& entry) {
    return entry.path == path && entry.status == status;
  });
}
}

int main() {
  const std::filesystem::path work = OFF_MANIFEST_TEST_WORK_DIR;
  if (work.empty() || work == work.root_path()) throw std::runtime_error("invalid manifest fixture directory");
  std::filesystem::remove_all(work);
  std::filesystem::create_directories(work);
  // Owning digest strings outlive every manifest string_view in this test.
  const auto game_hash = off::crypto::to_hex(off::crypto::sha256("game-data"));
  const auto bank_hash = off::crypto::to_hex(off::crypto::sha256("bank"));
  const auto music_hash = off::crypto::to_hex(off::crypto::sha256("music"));
  const auto support_hash = off::crypto::to_hex(off::crypto::sha256("support"));
  constexpr std::string_view track_a = "Freedom_Fighters_OST/FLAC/a.flac";
  constexpr std::string_view track_b = "Freedom_Fighters_OST/MP3/b.mp3";
  const std::array<ManifestFile, 5> manifest{{
      {"Scenes/payload.bin", 9, game_hash, ManifestFileRole::required_game},
      {"streams.wav", 4, bank_hash, ManifestFileRole::required_game},
      {track_a, 5, music_hash, ManifestFileRole::optional_soundtrack},
      {track_b, 5, music_hash, ManifestFileRole::optional_soundtrack},
      {"support.txt", 7, support_hash, ManifestFileRole::optional_support}}};
  const auto fixture = [&](std::string_view name) {
    const auto root = work / name;
    write(root / "Scenes/payload.bin", "game-data");
    write(root / "streams.wav", "bank");
    return root;
  };
  {
    const auto root = fixture("required-only");
    const auto result = verify_file_manifest(root, manifest);
    check(result.required_ok() && !result.cancelled && result.files.size() == manifest.size(),
          "required files verify without installing optional files");
    check(has(result, "Scenes/payload.bin", ManifestFileStatus::verified) &&
          has(result, "streams.wav", ManifestFileStatus::verified) &&
          has(result, track_a, ManifestFileStatus::missing) && has(result, track_b, ManifestFileStatus::missing),
          "required hashes and missing optional tracks retain separate statuses");
    write(root / track_a, "music");
    auto partial = verify_file_manifest(root, manifest);
    check(partial.required_ok() && has(partial, track_a, ManifestFileStatus::verified) &&
          has(partial, track_b, ManifestFileStatus::missing), "partial soundtrack installation is allowed");
    write(root / track_b, "music");
    write(root / "support.txt", "support");
    auto complete = verify_file_manifest(root, manifest);
    check(complete.required_ok() && std::all_of(complete.files.begin(), complete.files.end(),
          [](const auto& f) { return f.status == ManifestFileStatus::verified; }), "complete independent inventory verifies");
    write(root / track_a, "MUSIC");
    write(root / track_b, "x");
    write(root / "support.txt", "SUPPORT");
    const auto bad_optional = verify_file_manifest(root, manifest);
    check(bad_optional.required_ok() && has(bad_optional, track_a, ManifestFileStatus::hash_mismatch) &&
          has(bad_optional, track_b, ManifestFileStatus::size_mismatch) &&
          has(bad_optional, "support.txt", ManifestFileStatus::hash_mismatch),
          "corrupt optional soundtrack/support never fail required data");
    write(root / "Freedom_Fighters_OST/extra.flac", "unrecognized");
    write(root / "savegame.bin", "mutable");
    write(root / "Freedom_Fighters_OST/FF_OST_MP3_320/Thumbs.db", "cache");
    const auto unknown = verify_file_manifest(root, manifest);
    check(unknown.required_ok() && has(unknown, "Freedom_Fighters_OST/extra.flac", ManifestFileStatus::unexpected),
          "unknown soundtrack is reported without enrollment or mandatory failure");
    check(std::none_of(unknown.files.begin(), unknown.files.end(), [](const auto& f) {
      return f.path == "savegame.bin" || f.path.ends_with("Thumbs.db");
    }), "unrelated mutable root files and explicit thumbnail cache stay outside inventory");
  }
  {
    const auto root = fixture("required-changes");
    check(verify_file_manifest(root, manifest).required_ok(), "initial required bytes verify");
    const auto path = root / "Scenes/payload.bin";
    const auto time = std::filesystem::last_write_time(path);
    write(path, "GAME-DATA");
    std::filesystem::last_write_time(path, time);
    const auto edited = verify_file_manifest(root, manifest);
    check(!edited.required_ok() && has(edited, "Scenes/payload.bin", ManifestFileStatus::hash_mismatch),
          "same-size edit with restored mtime is rehashed on the next invocation");
    write(path, "x");
    check(has(verify_file_manifest(root, manifest), "Scenes/payload.bin", ManifestFileStatus::size_mismatch),
          "required size mismatch is diagnosed separately");
    std::filesystem::remove(path);
    const auto missing = verify_file_manifest(root, manifest);
    check(!missing.required_ok() && has(missing, "Scenes/payload.bin", ManifestFileStatus::missing),
          "missing required file fails");
    write(path, "game-data");
    write(root / "Scenes/new.bin", "new");
    const auto extra = verify_file_manifest(root, manifest);
    check(!extra.required_ok() && has(extra, "Scenes/new.bin", ManifestFileStatus::unexpected),
          "unexpected Scenes file fails rather than being trusted automatically");
  }
  {
    const auto root = work / "case-fold";
    write(root / "sCeNeS/PAYLOAD.BIN", "game-data");
    write(root / "STREAMS.WAV", "bank");
    check(verify_file_manifest(root, manifest).required_ok(), "manifest lookup folds path case including parent directory");
    write(root / "streams.wav", "bank");
    if (!std::filesystem::equivalent(root / "streams.wav", root / "STREAMS.WAV")) {
      const auto collision = verify_file_manifest(root, manifest);
      check(!collision.required_ok() && has(collision, "streams.wav", ManifestFileStatus::ambiguous),
            "equal-content case collision still rejects");
    } else std::cout << "SKIP: case-collision fixture needs a case-sensitive filesystem\n";
  }
  {
    const auto root = work / "declared-ancestors";
    write(root / "Extras/Nested/reference.bin", "game-data");
    write(root / "Optional/Booklet/text.txt", "support");
    const std::array<ManifestFile, 2> nested{{
        {"extras/nested/reference.bin", 9, game_hash, ManifestFileRole::required_game},
        {"optional/booklet/text.txt", 7, support_hash, ManifestFileRole::optional_support}}};
    const auto result = verify_file_manifest(root, nested);
    check(result.required_ok() && has(result, "extras/nested/reference.bin", ManifestFileStatus::verified) &&
          has(result, "optional/booklet/text.txt", ManifestFileStatus::verified),
          "declared ancestor directories outside built-in namespaces are traversed");
    write(root / "OPTIONAL/other.txt", "not the declared child");
    if (!std::filesystem::equivalent(root / "OPTIONAL", root / "Optional")) {
      const auto collision = verify_file_manifest(root, nested);
      check(collision.required_ok() && has(collision, "optional/booklet/text.txt", ManifestFileStatus::ambiguous) &&
            !has(collision, "optional/booklet/text.txt", ManifestFileStatus::verified),
            "optional parent collision disables its unique descendant without failing required data");
    } else std::cout << "SKIP: optional parent collision needs case-sensitive filesystem\n";
    const auto ost_root = fixture("ost-parent-collision");
    write(ost_root / track_a, "music");
    std::filesystem::create_directories(ost_root / "FREEDOM_FIGHTERS_OST");
    if (!std::filesystem::equivalent(ost_root / "FREEDOM_FIGHTERS_OST", ost_root / "Freedom_Fighters_OST")) {
      const auto collision = verify_file_manifest(ost_root, manifest);
      check(collision.required_ok() && has(collision, track_a, ManifestFileStatus::ambiguous) &&
            !has(collision, track_a, ManifestFileStatus::verified),
            "soundtrack parent collision cannot certify a child present in only one spelling");
    } else std::cout << "SKIP: soundtrack parent collision needs case-sensitive filesystem\n";
  }
  {
    const auto root = fixture("manifest-validation");
    for (const auto path : std::array<std::string_view, 10>{
        "", ".", "..", "../outside", "/absolute", "C:/drive", "Scenes\\payload.bin",
        "Scenes//payload.bin", "Scenes/../payload.bin", std::string_view{"a\0b", 3}}) {
      const std::array<ManifestFile, 1> invalid{{{path, 9, game_hash, ManifestFileRole::required_game}}};
      rejects([&] { (void)verify_file_manifest(root, invalid); }, "unsafe manifest path rejects");
    }
    for (const auto digest : {std::string{}, std::string(63, 'a'), std::string(65, 'a'),
                              std::string(64, 'g'), std::string(64, 'A')}) {
      const std::array<ManifestFile, 1> invalid{{{"streams.wav", 4, digest, ManifestFileRole::required_game}}};
      rejects([&] { (void)verify_file_manifest(root, invalid); }, "invalid digest rejects before checking files");
    }
    const std::array<ManifestFile, 2> duplicate{{manifest[1],
        {"STREAMS.WAV", 4, bank_hash, ManifestFileRole::required_game}}};
    rejects([&] { (void)verify_file_manifest(root, duplicate); }, "case-folded duplicate manifest entries reject");
    const std::array<ManifestFile, 2> parent_file{{
        {"Extras", 4, bank_hash, ManifestFileRole::optional_support},
        {"extras/child", 4, bank_hash, ManifestFileRole::optional_support}}};
    rejects([&] { (void)verify_file_manifest(root, parent_file); }, "manifest file cannot also be a declared parent");
    const std::array<ManifestFile, 1> invalid_role{{
        {"streams.wav", 4, bank_hash, static_cast<ManifestFileRole>(999)}}};
    rejects([&] { (void)verify_file_manifest(root, invalid_role); }, "invalid manifest role rejects");
  }
  {
    const auto root = fixture("links");
    const auto outside = work / "outside";
    write(outside / "payload.bin", "game-data");
    write(outside / "a.flac", "music");
    std::filesystem::remove(root / "Scenes/payload.bin");
    std::error_code file_error, directory_error;
    std::filesystem::create_symlink(std::filesystem::absolute(outside / "payload.bin"),
                                   root / "Scenes/payload.bin", file_error);
    std::filesystem::create_directory_symlink(std::filesystem::absolute(outside),
                                              root / "Freedom_Fighters_OST", directory_error);
    const auto result = verify_file_manifest(root, manifest);
    if (!file_error) check(!result.required_ok() && has(result, "Scenes/payload.bin", ManifestFileStatus::unsafe),
                          "required symlink is not trusted even with matching target bytes");
    if (!directory_error) {
      check(has(result, "Freedom_Fighters_OST", ManifestFileStatus::unsafe) &&
            has(result, track_a, ManifestFileStatus::missing) &&
            !has(result, "Freedom_Fighters_OST/a.flac", ManifestFileStatus::unexpected),
            "optional interior directory symlink is reported without following descendants");
      if (file_error) write(root / "Scenes/payload.bin", "game-data");
      else { std::filesystem::remove(root / "Scenes/payload.bin"); write(root / "Scenes/payload.bin", "game-data"); }
      check(verify_file_manifest(root, manifest).required_ok(), "unsafe optional directory never becomes a required failure");
    }
    if (file_error || directory_error) std::cout << "SKIP: unavailable symbolic-link fixture: "
        << file_error.message() << "; " << directory_error.message() << '\n';
  }
  {
    const auto root = fixture("cancel-immediate");
    unsigned calls = 0;
    const auto stopped = verify_file_manifest(root, manifest, [&] { ++calls; return true; });
    check(stopped.cancelled && !stopped.required_ok() && stopped.files.empty() && calls == 1,
          "immediate cancellation stops before enumeration and is never success");
    const auto large_root = work / "cancel-hash";
    const std::string bytes(200'000, 'q');
    write(large_root / "streams.wav", bytes);
    const auto digest = off::crypto::to_hex(off::crypto::sha256(bytes));
    const std::array<ManifestFile, 1> large{{{"streams.wav", bytes.size(), digest, ManifestFileRole::required_game}}};
    calls = 0;
    const auto mid_hash = verify_file_manifest(large_root, large, [&] { return ++calls >= 5; });
    check(mid_hash.cancelled && !mid_hash.required_ok() && calls <= 6 && mid_hash.files.empty(),
          "cancellation during streamed hashing stops without a verified result");
    calls = 0;
    const auto pulse = verify_file_manifest(large_root, large, [&] { return ++calls == 5; });
    check(pulse.cancelled && !pulse.required_ok() && calls == 5 && pulse.files.empty(),
          "one-shot cancellation inside hashing remains latched without polling again");
  }
  return failures == 0 ? 0 : 1;
}
