#pragma once

#include <filesystem>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

enum class ManifestFileRole { required_game, optional_soundtrack, optional_support };
struct ManifestFile {
  std::string path;
  std::uintmax_t size;
  // Empty means capture the current local digest during discovery. Nonempty
  // values are retained for synthetic verifier callers only.
  std::string sha256;
  ManifestFileRole role;
};
enum class ManifestFileStatus { verified, missing, unsafe, ambiguous, size_mismatch, hash_mismatch, io_error, unexpected };
struct ManifestFileCheck {
  std::string path;
  std::filesystem::path actual_path;
  ManifestFileRole role;
  ManifestFileStatus status;
  std::string detail;
  std::string sha256;
};
struct ManifestVerification {
  std::vector<ManifestFileCheck> files;
  bool cancelled{false};
  [[nodiscard]] bool required_ok() const noexcept;
};

// Discover the current user-owned tree. The inventory contains the executable,
// required stream bank, files below Scenes, and locally present soundtrack files.
[[nodiscard]] std::vector<ManifestFile> discover_local_install_manifest(
    const std::filesystem::path& root, const std::filesystem::path& executable,
    const std::function<bool()>& cancelled = {});

// A content-free, versioned identifier for locally observed required files.
// This is a consistency identity, not publisher authenticity.
[[nodiscard]] std::string verified_data_manifest_fingerprint(
    std::span<const ManifestFile> manifest);

// Full-byte hashes on every call; never trust timestamps. Hash worker count is
// bounded. Unknown files outside discovered roots are not part of the identity.
// Root symlink is canonicalized; interior symlinks are never followed. The
// installation must remain stable during verification and subsequent use.
[[nodiscard]] ManifestVerification verify_file_manifest(
    const std::filesystem::path& root, std::span<const ManifestFile> manifest,
    const std::function<bool()>& cancelled = {},
    // Zero selects the bounded production default of four workers.  A
    // positive value is capped so a corrupt installation cannot turn startup
    // verification into an unbounded thread creator.
    std::size_t hash_workers = 0);

} // namespace off::data
