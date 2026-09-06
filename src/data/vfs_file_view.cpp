#include "off/data/vfs_file_view.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace off::data {

struct VfsFileReader::Impl {
    std::ifstream input;
    std::uint64_t size{};
    bool failed{};
};

VfsFileReader::VfsFileReader(const std::filesystem::path& path, std::uint64_t expected_size)
    : impl_(std::make_unique<Impl>()) {
    std::error_code error;
    const auto status=std::filesystem::symlink_status(path,error);
    if (error || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
        throw std::runtime_error("streaming VFS source is not a regular file");
    impl_->input.open(path,std::ios::binary | std::ios::ate);
    if (!impl_->input) throw std::runtime_error("could not open retained streaming VFS source");
    const auto end=impl_->input.tellg();
    if (end<0 || static_cast<std::uint64_t>(end)!=expected_size)
        throw std::runtime_error("opened streaming VFS source size differs from mounted size");
    impl_->size=expected_size;
}
VfsFileReader::~VfsFileReader() = default;
VfsFileReader::VfsFileReader(VfsFileReader&&) noexcept = default;
VfsFileReader& VfsFileReader::operator=(VfsFileReader&&) noexcept = default;
std::uint64_t VfsFileReader::size() const noexcept { return impl_ ? impl_->size : 0; }
void VfsFileReader::read_at(std::uint64_t offset,std::span<std::byte> destination) {
    if (!impl_ || impl_->failed) throw std::runtime_error("streaming VFS reader is unavailable");
    if (offset>impl_->size || destination.size()>impl_->size-offset ||
        offset>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        destination.size()>static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("retained streaming VFS read exceeds bounds");
    if (destination.empty()) return;
    // Recheck the retained handle, not its possibly replaced pathname. Some
    // standard-library file buffers can retain bytes across an external
    // truncation; gcount alone does not reliably detect that size change.
    impl_->input.seekg(0,std::ios::end);
    const auto current_end=impl_->input.tellg();
    if (current_end<0 || static_cast<std::uint64_t>(current_end)!=impl_->size) {
        impl_->failed=true;
        throw std::runtime_error("retained streaming VFS source size changed");
    }
    impl_->input.seekg(static_cast<std::streamoff>(offset));
    impl_->input.read(reinterpret_cast<char*>(destination.data()),
                      static_cast<std::streamsize>(destination.size()));
    if (!impl_->input || impl_->input.gcount()!=static_cast<std::streamsize>(destination.size())) {
        impl_->failed=true;
        throw std::runtime_error("could not complete retained streaming VFS read");
    }
}
VfsFileReader VfsFileView::open_reader() const { return VfsFileReader(path_,size_); }

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
