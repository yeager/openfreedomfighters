#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace off::data {

class ArchiveVfs;

class VfsFileView final {
public:
    [[nodiscard]] std::uint64_t size() const noexcept {
        return size_;
    }

    void read_at(std::uint64_t offset, std::span<std::byte> destination) const;
    [[nodiscard]] std::vector<std::byte> read(
        std::uint64_t offset,
        std::size_t length
    ) const;

private:
    friend class ArchiveVfs;

    VfsFileView(std::filesystem::path path, std::uint64_t size)
        : path_(std::move(path)), size_(size) {}

    std::filesystem::path path_;
    std::uint64_t size_{0};
};

}  // namespace off::data
