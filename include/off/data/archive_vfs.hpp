#pragma once

#include "off/data/zip_archive.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace off::data {

class ArchiveVfs final {
public:
    using MountId = std::uint64_t;

    [[nodiscard]] MountId mount_archive(const std::filesystem::path& archive);
    [[nodiscard]] MountId mount_directory(const std::filesystem::path& directory);
    [[nodiscard]] MountId mount(const std::filesystem::path& archive) {
        return mount_archive(archive);
    }
    [[nodiscard]] bool unmount(MountId id) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::size_t mount_count() const noexcept {
        return mounts_.size();
    }
    [[nodiscard]] bool contains(std::string_view path) const;
    [[nodiscard]] std::vector<std::byte> read(std::string_view path) const;

private:
    struct Mount {
        MountId id{0};
        std::filesystem::path directory;
        std::unordered_map<std::string, std::filesystem::path> loose_files;
        std::optional<ZipArchive> archive;
    };

    std::vector<Mount> mounts_;
    MountId next_mount_id_{1};
};

}  // namespace off::data
