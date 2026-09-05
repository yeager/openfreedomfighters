#include "off/data/scene_support.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::uint32_t dlcf_magic = 0x46434c44U;
constexpr std::uint32_t flag_mask = 0xc0000000U;
constexpr std::uint32_t size_mask = 0x3fffffffU;
constexpr std::uint32_t root_flag = 0x80000000U;
constexpr std::uint32_t array_flag = 0x40000000U;
constexpr std::size_t root_size = 16;
constexpr std::size_t maximum_file_size = 1024U * 1024U;
constexpr std::uint32_t maximum_dependencies = 4096;
constexpr std::uint32_t maximum_name_size = 4096;

[[nodiscard]] std::string dependency_name(std::span<const std::byte> field) {
    if (field.empty() || field.back() != std::byte{0}) {
        throw std::runtime_error("scene-support dependency is not NUL terminated");
    }
    const auto name = field.first(field.size() - 1);
    if (std::find(name.begin(), name.end(), std::byte{0}) != name.end()) {
        throw std::runtime_error("scene-support dependency contains an embedded NUL");
    }
    if (!std::all_of(name.begin(), name.end(), [](std::byte value) {
            const auto character = std::to_integer<unsigned int>(value);
            return character >= 0x20U && character <= 0x7eU;
        })) {
        throw std::runtime_error("scene-support dependency is not printable ASCII");
    }
    return {
        reinterpret_cast<const char*>(name.data()),
        name.size(),
    };
}

}  // namespace

SceneSupport SceneSupport::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < 25 || bytes.size() > maximum_file_size ||
        (bytes.size() & 3U) != 0U) {
        throw std::runtime_error("invalid scene-support file size");
    }
    const ByteReader reader(bytes);
    const auto file_size = static_cast<std::uint32_t>(bytes.size());
    if (reader.u32(0) != 0 || reader.u32(4) != (root_flag | file_size) ||
        reader.u32(8) != file_size || reader.u32(12) != 1) {
        throw std::runtime_error("invalid scene-support root envelope");
    }
    if (reader.u32(root_size) != dlcf_magic) {
        throw std::runtime_error("scene-support DLCF block was not found");
    }

    const auto descriptor = reader.u32(root_size + 4);
    if ((descriptor & size_mask) != file_size - root_size) {
        throw std::runtime_error("scene-support DLCF block size mismatch");
    }

    SceneSupport result;
    const auto flags = descriptor & flag_mask;
    if (flags == 0) {
        result.dependencies_.push_back(dependency_name(bytes.subspan(root_size + 8)));
        return result;
    }
    if (flags != array_flag || bytes.size() < root_size + 16) {
        throw std::runtime_error("unsupported scene-support DLCF block layout");
    }

    const auto data_offset = reader.u32(root_size + 8);
    const auto count = reader.u32(root_size + 12);
    if (count == 0 || count > maximum_dependencies ||
        data_offset != 16U + count * 4U) {
        throw std::runtime_error("invalid scene-support dependency table");
    }
    const auto data_start = root_size + static_cast<std::size_t>(data_offset);
    if (data_start > bytes.size()) {
        throw std::runtime_error("scene-support dependency table exceeds input bounds");
    }

    result.dependencies_.reserve(count);
    auto cursor = data_start;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto length = reader.u32(root_size + 16U + index * 4U);
        if (length == 0 || length > maximum_name_size ||
            length > bytes.size() - cursor) {
            throw std::runtime_error("invalid scene-support dependency length");
        }
        result.dependencies_.push_back(dependency_name(reader.slice(cursor, length)));
        cursor += length;
    }
    const auto padding = bytes.subspan(cursor);
    if (padding.size() >= 4 ||
        !std::all_of(padding.begin(), padding.end(), [](std::byte value) {
            return value == std::byte{0};
        })) {
        throw std::runtime_error("invalid scene-support string-table padding");
    }
    return result;
}

}  // namespace off::data
