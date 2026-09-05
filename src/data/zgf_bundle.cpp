#include "off/data/zgf_bundle.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace off::data {
namespace {

constexpr std::uint32_t zgf_magic = 0x5a474654U;
constexpr std::uint32_t flag_mask = 0xc0000000U;
constexpr std::uint32_t size_mask = 0x3fffffffU;
constexpr std::uint32_t root_flag = 0x80000000U;
constexpr std::uint32_t entry_type = 1;
constexpr std::size_t root_size = 16;
constexpr std::size_t empty_root_size = 8;
constexpr std::size_t entry_header_size = 16;
constexpr std::size_t blob_header_size = 8;
constexpr std::uint32_t maximum_entries = 65'536;
constexpr std::uint32_t maximum_entry_payload = 128U * 1024U * 1024U;
constexpr std::uint32_t maximum_name_size = 4'096;
constexpr std::size_t maximum_name_field_size = maximum_name_size + 4U;

[[nodiscard]] bool is_safe_logical_path(std::string_view name) noexcept {
    if (name.empty() || name.front() == '/' || name.front() == '\\' ||
        name.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= name.size()) {
        const auto end = name.find_first_of("/\\", start);
        const auto segment = name.substr(
            start,
            end == std::string_view::npos ? name.size() - start : end - start
        );
        if (segment.empty() || segment == ".") {
            return false;
        }
        if (end == std::string_view::npos) {
            return true;
        }
        start = end + 1;
    }
    return true;
}

[[nodiscard]] std::size_t aligned_size(std::uint32_t value) {
    return (static_cast<std::size_t>(value) + 3U) & ~std::size_t{3};
}

void require_zero(std::span<const std::byte> bytes, const char* message) {
    if (!std::ranges::all_of(bytes, [](std::byte value) { return value == std::byte{0}; })) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] std::string read_name(std::span<const std::byte> field) {
    if (field.empty() || field.size() > maximum_name_field_size) {
        throw std::runtime_error("invalid ZGF entry-name field size");
    }
    const auto terminator = std::ranges::find(field, std::byte{0});
    if (terminator == field.end()) {
        throw std::runtime_error("ZGF entry name is not NUL terminated");
    }
    const auto size = static_cast<std::size_t>(terminator - field.begin());
    if (size == 0 || size > maximum_name_size) {
        throw std::runtime_error("invalid ZGF entry name size");
    }
    const auto name_bytes = field.first(size);
    if (!std::ranges::all_of(name_bytes, [](std::byte value) {
            const auto character = std::to_integer<unsigned int>(value);
            return character >= 0x20U && character <= 0x7eU;
        })) {
        throw std::runtime_error("ZGF entry name is not printable ASCII");
    }
    const auto padding = field.subspan(size + 1);
    if (padding.size() >= 4) {
        throw std::runtime_error("invalid ZGF entry-name alignment");
    }
    require_zero(padding, "invalid ZGF entry-name padding");
    std::string name(reinterpret_cast<const char*>(name_bytes.data()), name_bytes.size());
    if (!is_safe_logical_path(name)) {
        throw std::runtime_error("unsafe logical path in ZGF bundle");
    }
    return name;
}

[[nodiscard]] std::string normalized(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        const auto slash = character == '\\' ? '/' : character;
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(slash))
        ));
    }
    return result;
}

}  // namespace

ZgfBundle ZgfBundle::parse(PackedResource resource) {
    const auto bytes = resource.payload();
    if (bytes.size() < empty_root_size || (bytes.size() & 3U) != 0U) {
        throw std::runtime_error("invalid ZGF decoded size");
    }
    const ByteReader reader(bytes);
    const auto file_size = static_cast<std::uint32_t>(bytes.size());
    if (reader.u32(0) != zgf_magic) {
        throw std::runtime_error("invalid ZGF signature");
    }

    ZgfBundle result;
    if (bytes.size() == empty_root_size) {
        if (reader.u32(4) != empty_root_size) {
            throw std::runtime_error("invalid empty ZGF root");
        }
        result.resource_ = std::move(resource);
        return result;
    }
    if (bytes.size() < root_size || reader.u32(4) != (root_flag | file_size) ||
        reader.u32(8) != file_size) {
        throw std::runtime_error("invalid ZGF root envelope");
    }
    const auto entry_count = reader.u32(12);
    if (entry_count == 0 || entry_count > maximum_entries) {
        throw std::runtime_error("invalid ZGF entry count");
    }

    result.entries_.reserve(entry_count);
    std::unordered_set<std::string> names;
    std::size_t position = root_size;
    for (std::uint32_t index = 0; index < entry_count; ++index) {
        if (position > bytes.size() || entry_header_size + blob_header_size >
                bytes.size() - position) {
            throw std::runtime_error("ZGF entry header exceeds input bounds");
        }
        const auto descriptor = reader.u32(position + 4);
        const auto entry_size = descriptor & size_mask;
        if (reader.u32(position) != entry_type ||
            (descriptor & flag_mask) != root_flag ||
            entry_size < entry_header_size + blob_header_size ||
            entry_size > bytes.size() - position ||
            reader.u32(position + 12) != 1) {
            throw std::runtime_error("invalid ZGF entry envelope");
        }

        const auto blob_position = position + entry_header_size;
        const auto payload_size = reader.u32(blob_position);
        const auto blob_descriptor = reader.u32(blob_position + 4);
        const auto blob_size = blob_descriptor & size_mask;
        const auto expected_blob_size = blob_header_size + aligned_size(payload_size);
        const auto name_offset = reader.u32(position + 8);
        if (payload_size == 0 || payload_size > maximum_entry_payload ||
            (blob_descriptor & flag_mask) != 0 ||
            blob_size != expected_blob_size ||
            name_offset != entry_header_size + blob_size ||
            name_offset >= entry_size) {
            throw std::runtime_error("invalid ZGF embedded-payload envelope");
        }

        const auto payload_position = blob_position + blob_header_size;
        const auto payload_padding = aligned_size(payload_size) - payload_size;
        require_zero(
            reader.slice(payload_position + payload_size, payload_padding),
            "invalid ZGF embedded-payload padding"
        );
        auto name = read_name(reader.slice(position + name_offset, entry_size - name_offset));
        if (!names.insert(normalized(name)).second) {
            throw std::runtime_error("duplicate normalized path in ZGF bundle");
        }
        result.entries_.push_back({
            .name = std::move(name),
            .record_offset = static_cast<std::uint32_t>(position),
            .payload_offset = static_cast<std::uint32_t>(payload_position),
            .payload_size = payload_size,
        });
        position += entry_size;
    }
    if (position != bytes.size()) {
        throw std::runtime_error("ZGF entry chain does not cover the decoded resource");
    }
    result.resource_ = std::move(resource);
    return result;
}

std::span<const std::byte> ZgfBundle::entry_payload(std::size_t index) const {
    if (index >= entries_.size()) {
        throw std::runtime_error("ZGF entry index is out of range");
    }
    const auto& entry = entries_[index];
    return resource_.payload().subspan(entry.payload_offset, entry.payload_size);
}

std::optional<ZgfPayloadLocation> ZgfBundle::locate_payload_offset(
    std::uint32_t decoded_offset
) const noexcept {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];
        if (decoded_offset >= entry.payload_offset &&
            decoded_offset - entry.payload_offset < entry.payload_size) {
            return ZgfPayloadLocation{
                .entry_index = index,
                .offset_within_payload = decoded_offset - entry.payload_offset,
            };
        }
    }
    return std::nullopt;
}

}  // namespace off::data
