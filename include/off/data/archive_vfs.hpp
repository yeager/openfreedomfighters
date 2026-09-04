#pragma once

#include "off/data/zip_archive.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace off::data {

class ArchiveVfs final {
public:
    void mount(const std::filesystem::path& archive);

    [[nodiscard]] std::size_t mount_count() const noexcept {
        return mounts_.size();
    }
    [[nodiscard]] bool contains(std::string_view path) const noexcept;
    [[nodiscard]] std::vector<std::byte> read(std::string_view path) const;

private:
    std::vector<ZipArchive> mounts_;
};

}  // namespace off::data
