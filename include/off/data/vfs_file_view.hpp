#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace off::data {

class ArchiveVfs;

// Owning, move-only opened file. Reads stay on this file handle across path
// replacement. This is not an immutable snapshot: in-place writes remain outside
// the verified-installation contract. Use from one worker at a time.
class VfsFileReader final {
public:
    ~VfsFileReader();
    VfsFileReader(VfsFileReader&&) noexcept;
    VfsFileReader& operator=(VfsFileReader&&) noexcept;
    VfsFileReader(const VfsFileReader&) = delete;
    VfsFileReader& operator=(const VfsFileReader&) = delete;
    [[nodiscard]] std::uint64_t size() const noexcept;
    void read_at(std::uint64_t offset, std::span<std::byte> destination);
private:
    friend class VfsFileView;
    VfsFileReader(const std::filesystem::path& path, std::uint64_t expected_size);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class VfsFileView final {
public:
    [[nodiscard]] std::uint64_t size() const noexcept {
        return size_;
    }
    [[nodiscard]] VfsFileReader open_reader() const;

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
