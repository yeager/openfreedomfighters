#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace off::data {

enum class InstallError {
    none,
    missing_root,
    missing_executable,
    incomplete_game_data,
    io_error,
};

// A locally observed optional file identity, not a cue identity.
struct VerifiedSoundtrackCandidate {
    std::filesystem::path path;
    std::string expected_sha256;
};

struct InstallVerification {
    InstallError error{InstallError::none};
    std::filesystem::path root;
    std::filesystem::path executable;
    std::string executable_sha256;
    // Present after required local files and the structural audit pass. This
    // opaque fingerprint is not an authenticity claim and never a
    // directory, mission, asset, or payload fingerprint.
    std::string data_manifest_fingerprint;
    std::string message;
    // Hash-verified candidates only: decoder/cue suitability is not implied.
    std::vector<VerifiedSoundtrackCandidate> soundtrack_candidates;
    std::vector<std::string> optional_file_warnings;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == InstallError::none;
    }
};

struct InstallVerificationOptions {
    // Empty disables the derived deep-audit cache. The discovered local
    // inventory is fully hashed before this cache can be consulted.
    std::filesystem::path deep_audit_cache_root;
};

[[nodiscard]] InstallVerification verify_install(const std::filesystem::path& root,
    const std::function<bool()>& cancelled = {},
    const InstallVerificationOptions& options = {});

}  // namespace off::data
