#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

struct ZipEntry {
    std::string name;
    std::uint16_t flags{0};
    std::uint16_t compression_method{0};
    std::uint32_t crc32{0};
    std::uint32_t compressed_size{0};
    std::uint32_t uncompressed_size{0};
    std::uint32_t local_header_offset{0};
};

class ZipArchive final {
public:
    [[nodiscard]] static ZipArchive open(const std::filesystem::path& path);

    [[nodiscard]] std::span<const ZipEntry> entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const ZipEntry* find(std::string_view name) const;
    [[nodiscard]] std::vector<std::byte> read(const ZipEntry& entry) const;

private:
    std::vector<std::byte> bytes_;
    std::vector<ZipEntry> entries_;
};

[[nodiscard]] bool is_safe_archive_path(std::string_view name) noexcept;

}  // namespace off::data
