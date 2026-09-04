#include "off/data/archive_vfs.hpp"

#include <stdexcept>

namespace off::data {

void ArchiveVfs::mount(const std::filesystem::path& archive) {
    mounts_.push_back(ZipArchive::open(archive));
}

bool ArchiveVfs::contains(std::string_view path) const noexcept {
    for (auto mount = mounts_.rbegin(); mount != mounts_.rend(); ++mount) {
        if (mount->find(path) != nullptr) {
            return true;
        }
    }
    return false;
}

std::vector<std::byte> ArchiveVfs::read(std::string_view path) const {
    if (!is_safe_archive_path(path)) {
        throw std::runtime_error("unsafe virtual path");
    }
    for (auto mount = mounts_.rbegin(); mount != mounts_.rend(); ++mount) {
        if (const auto* entry = mount->find(path); entry != nullptr) {
            return mount->read(*entry);
        }
    }
    throw std::runtime_error("virtual file was not found");
}

}  // namespace off::data
