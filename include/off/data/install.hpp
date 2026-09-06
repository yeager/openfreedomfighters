#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace off::data {

inline constexpr std::uintmax_t supported_executable_size = 3'353'592;
inline constexpr auto supported_executable_sha256 =
    "b05ca73c44474320e1b7321c24be66270f1de2a0686b063b6cb18e9ed21de9c9";

enum class InstallError {
    none,
    missing_root,
    missing_executable,
    unsupported_executable_size,
    unsupported_executable_hash,
    incomplete_game_data,
    io_error,
};

struct InstallVerification {
    InstallError error{InstallError::none};
    std::filesystem::path root;
    std::filesystem::path executable;
    std::string executable_sha256;
    std::string message;
    // Hash-verified candidates only: decoder/cue suitability is not implied.
    std::vector<std::filesystem::path> soundtrack_candidates;
    std::vector<std::string> optional_file_warnings;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == InstallError::none;
    }
};

[[nodiscard]] InstallVerification verify_install(const std::filesystem::path& root,
    const std::function<bool()>& cancelled = {});

}  // namespace off::data
