#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace off::crypto {

using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256 final {
public:
    Sha256();

    void update(std::span<const std::byte> bytes);
    [[nodiscard]] Sha256Digest finish();

private:
    void transform(const std::byte* block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64> buffer_{};
    std::uint64_t total_bytes_{0};
    std::size_t buffered_{0};
    bool finished_{false};
};

[[nodiscard]] Sha256Digest sha256(std::string_view text);
[[nodiscard]] Sha256Digest sha256_file(const std::filesystem::path& path);
[[nodiscard]] std::string to_hex(const Sha256Digest& digest);

}  // namespace off::crypto

