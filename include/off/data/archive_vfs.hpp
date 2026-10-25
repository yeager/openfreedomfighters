#pragma once

#include "off/data/zip_archive.hpp"
#include "off/data/vfs_file_view.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace off::data {

class ArchiveVfs final {
public:
    using MountId = std::uint64_t;

    [[nodiscard]] MountId mount_archive(const std::filesystem::path& archive);
    // Exclusions name single top-level entries, matched with VFS case folding.
    // Matched entries are neither inspected nor traversed/indexed, and do not
    // contribute to mount size limits. No exclusions preserves strict mounting.
    [[nodiscard]] MountId mount_directory(
        const std::filesystem::path& directory,
        std::span<const std::string_view> excluded_top_level = {});
    // Includes only the selected single top-level entries, matched with VFS
    // case folding. Unselected entries are not inspected or traversed. An
    // empty selection creates a valid empty mount after validating the root.
    [[nodiscard]] MountId mount_directory_subset(
        const std::filesystem::path& directory,
        std::span<const std::string_view> included_top_level);
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
    [[nodiscard]] VfsFileView open_stream(std::string_view path) const;

private:
    struct Mount {
        MountId id{0};
        std::filesystem::path directory;
        std::unordered_map<std::string, std::filesystem::path> loose_files;
        std::optional<ZipArchive> archive;
    };

    std::vector<Mount> mounts_;
    MountId next_mount_id_{1};

    [[nodiscard]] MountId mount_directory_impl(
        const std::filesystem::path& directory,
        std::span<const std::string_view> selected_top_level,
        bool include_only);
};

}  // namespace off::data
