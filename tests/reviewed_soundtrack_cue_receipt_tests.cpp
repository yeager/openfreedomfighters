#include "off/audio/reviewed_soundtrack_cue_receipt.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(const bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  check(static_cast<bool>(output), "fixture write succeeds");
}
// Authored source-free receipt fixture; no retail content is included.
std::string receipt() {
  return R"({"format":"off.reviewed-soundtrack-cue-bindings/v1","verified_data_manifest_fingerprint":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","bindings":[{"cue_token":11,"album_ordinal":1,"format":"flac","expected_sha256":"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"},{"cue_token":22,"album_ordinal":2,"format":"mp3","expected_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}]})";
}
}

int main() {
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR};
    const auto directory = root / "reviewed-soundtrack-cue";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    const auto path = directory / "reviewed-soundtrack-cue-bindings.json";
    write(path, receipt());
    const auto admitted = off::audio::ReviewedSoundtrackCueReceipt::load_local(directory);
    check(admitted && admitted->bindings().size() == 2U, "exact receipt is admitted");
    check(admitted->bindings()[0].cue_token == 11U && admitted->bindings()[0].album_ordinal == 1U &&
              admitted->bindings()[0].format == off::audio::SoundtrackFormat::flac,
          "only opaque binding identity is retained");
    auto duplicate = receipt();
    duplicate.replace(duplicate.rfind("\"cue_token\":22"), 14U, "\"cue_token\":11");
    write(path, duplicate);
    check(!off::audio::ReviewedSoundtrackCueReceipt::load_local(directory), "duplicate opaque tokens are rejected");
    auto bad_digest = receipt();
    bad_digest.replace(bad_digest.find("abcdef0123456789"), 64U, 64U, 'A');
    write(path, bad_digest);
    check(!off::audio::ReviewedSoundtrackCueReceipt::load_local(directory), "non-lowercase digest is rejected");
    write(path, receipt() + std::string(1, '\0'));
    check(!off::audio::ReviewedSoundtrackCueReceipt::load_local(directory), "trailing bytes are rejected");
    write(path, R"({"format":"off.reviewed-soundtrack-cue-bindings/v1","bindings":[]})");
    check(!off::audio::ReviewedSoundtrackCueReceipt::load_local(directory), "incomplete receipt is rejected");
    write(path, receipt());
    const auto target = root / "private-receipt.json";
    std::filesystem::rename(path, target, error);
    check(!error, "fixture moves");
    std::filesystem::create_symlink(target, path, error);
    check(!error, "symlink fixture exists");
    check(!off::audio::ReviewedSoundtrackCueReceipt::load_local(directory), "symlink receipt is rejected");
    std::filesystem::remove_all(root, error);
    std::cout << "reviewed soundtrack cue receipt tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
