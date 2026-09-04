#include "off/data/vfs_file_view.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace off::data {

void VfsFileView::read_at(
    std::uint64_t offset,
    std::span<std::byte> destination
) const {
    if (offset > size_ || destination.size() > size_ - offset) {
        throw std::runtime_error("streaming VFS read exceeds file bounds");
    }

    std::error_code error;
    const auto status = std::filesystem::symlink_status(path_, error);
    const auto current_size = std::filesystem::file_size(path_, error);
    if (error || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status) || current_size != size_) {
        throw std::runtime_error("streaming VFS source changed after it was mounted");
    }
    if (destination.empty()) {
        return;
    }
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error("streaming VFS offset is not representable on this platform");
    }

    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open streaming VFS source");
    }
    input.seekg(static_cast<std::streamoff>(offset));
    input.read(
        reinterpret_cast<char*>(destination.data()),
        static_cast<std::streamsize>(destination.size())
    );
    if (!input) {
        throw std::runtime_error("could not complete streaming VFS read");
    }
}

std::vector<std::byte> VfsFileView::read(
    std::uint64_t offset,
    std::size_t length
) const {
    if (offset > size_ || length > size_ - offset) {
        throw std::runtime_error("streaming VFS read exceeds file bounds");
    }
    std::vector<std::byte> bytes(length);
    read_at(offset, bytes);
    return bytes;
}

}  // namespace off::data
