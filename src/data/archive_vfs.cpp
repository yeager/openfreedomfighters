#include "off/data/archive_vfs.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::uintmax_t maximum_loose_file_size = 256ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t maximum_directory_size = 1024ULL * 1024ULL * 1024ULL;

[[nodiscard]] std::string normalized(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        const auto slash = character == '\\' ? '/' : character;
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(slash))));
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> read_loose_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
        throw std::runtime_error("loose VFS entry is no longer a regular file");
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maximum_loose_file_size ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("loose VFS entry exceeds the safety size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open loose VFS entry");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input) {
        throw std::runtime_error("could not read loose VFS entry");
    }
    return bytes;
}

}  // namespace

ArchiveVfs::MountId ArchiveVfs::mount_archive(const std::filesystem::path& archive) {
    Mount mount;
    mount.id = next_mount_id_++;
    mount.archive = ZipArchive::open(archive);
    mounts_.push_back(std::move(mount));
    return mounts_.back().id;
}

ArchiveVfs::MountId ArchiveVfs::mount_directory(const std::filesystem::path& directory) {
    std::error_code error;
    const auto root = std::filesystem::canonical(directory, error);
    if (error || !std::filesystem::is_directory(root)) {
        throw std::runtime_error("could not open VFS directory mount");
    }

    Mount mount;
    mount.id = next_mount_id_++;
    mount.directory = root;
    std::uintmax_t total_size = 0;
    for (std::filesystem::recursive_directory_iterator iterator(root), end;
         iterator != end;
         ++iterator) {
        const auto status = iterator->symlink_status();
        if (std::filesystem::is_symlink(status)) {
            throw std::runtime_error("VFS directory mounts cannot contain symbolic links");
        }
        if (!std::filesystem::is_regular_file(status)) {
            continue;
        }
        const auto size = iterator->file_size();
        if (size > maximum_loose_file_size) {
            throw std::runtime_error("loose VFS entry exceeds the safety size limit");
        }
        total_size += size;
        if (total_size > maximum_directory_size) {
            throw std::runtime_error("VFS directory mount exceeds the total safety size limit");
        }
        const auto relative = std::filesystem::relative(iterator->path(), root).generic_string();
        if (!is_safe_archive_path(relative)) {
            throw std::runtime_error("unsafe path in VFS directory mount");
        }
        if (!mount.loose_files.emplace(normalized(relative), iterator->path()).second) {
            throw std::runtime_error("duplicate normalized path in VFS directory mount");
        }
    }
    mounts_.push_back(std::move(mount));
    return mounts_.back().id;
}

bool ArchiveVfs::unmount(MountId id) noexcept {
    const auto found = std::find_if(mounts_.begin(), mounts_.end(), [id](const Mount& mount) {
        return mount.id == id;
    });
    if (found == mounts_.end()) {
        return false;
    }
    mounts_.erase(found);
    return true;
}

void ArchiveVfs::clear() noexcept {
    mounts_.clear();
}

bool ArchiveVfs::contains(std::string_view path) const {
    if (!is_safe_archive_path(path)) {
        return false;
    }
    const auto wanted = normalized(path);
    for (auto mount = mounts_.rbegin(); mount != mounts_.rend(); ++mount) {
        if (mount->loose_files.contains(wanted) ||
            (mount->archive.has_value() && mount->archive->find(path) != nullptr)) {
            return true;
        }
    }
    return false;
}

std::vector<std::byte> ArchiveVfs::read(std::string_view path) const {
    if (!is_safe_archive_path(path)) {
        throw std::runtime_error("unsafe virtual path");
    }
    const auto wanted = normalized(path);
    for (auto mount = mounts_.rbegin(); mount != mounts_.rend(); ++mount) {
        if (const auto loose = mount->loose_files.find(wanted);
            loose != mount->loose_files.end()) {
            return read_loose_file(loose->second);
        }
        if (mount->archive.has_value()) {
            if (const auto* entry = mount->archive->find(path); entry != nullptr) {
                return mount->archive->read(*entry);
            }
        }
    }
    throw std::runtime_error("virtual file was not found");
}

}  // namespace off::data
