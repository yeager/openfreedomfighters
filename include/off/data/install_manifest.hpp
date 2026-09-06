#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

enum class ManifestFileRole { required_game, optional_soundtrack, optional_support };
struct ManifestFile {
  std::string_view path;
  std::uintmax_t size;
  std::string_view sha256;
  ManifestFileRole role;
};
enum class ManifestFileStatus { verified, missing, unsafe, ambiguous, size_mismatch, hash_mismatch, io_error, unexpected };
struct ManifestFileCheck {
  std::string path;
  std::filesystem::path actual_path;
  ManifestFileRole role;
  ManifestFileStatus status;
  std::string detail;
};
struct ManifestVerification {
  std::vector<ManifestFileCheck> files;
  bool cancelled{false};
  [[nodiscard]] bool required_ok() const noexcept;
};

// Fixed local reference snapshot, not an official Steam authenticity manifest.
[[nodiscard]] std::span<const ManifestFile> supported_install_manifest() noexcept;

// Full-byte hashes on every call; never enroll/rebaseline or trust timestamps.
// Optional failures remain reports, not required-data failures. Unknown files
// in Scenes reject; unknown OST files remain unverified optional sources. Saves,
// configuration and unrelated root files are outside the immutable inventory.
// Root symlink is canonicalized; interior symlinks are never followed. The
// installation must remain stable during verification and subsequent use.
[[nodiscard]] ManifestVerification verify_file_manifest(
    const std::filesystem::path& root, std::span<const ManifestFile> manifest,
    const std::function<bool()>& cancelled = {});

} // namespace off::data
