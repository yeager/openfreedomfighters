#include "off/data/gms_image.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace off::data {
namespace {

constexpr std::uint32_t reference_flag_mask = 0xc0000000U;
constexpr std::uint32_t gms_reference_flag = 0x40000000U;
constexpr std::uint32_t reference_offset_mask = 0x3fffffffU;
constexpr std::uint32_t record_word_offset_mask = 0x00ffffffU;
constexpr std::uint32_t record_flag = 0x01000000U;
constexpr std::size_t fixed_header_size = 32;
constexpr std::size_t directory_entry_size = 8;
constexpr std::size_t minimum_record_size = 48;
constexpr std::size_t object_slot_size = 112;

[[nodiscard]] std::size_t checked_table_end(
    std::size_t offset,
    std::size_t count,
    std::size_t entry_size,
    std::size_t payload_size,
    const char* message
) {
    if (offset > payload_size || count > (payload_size - offset) / entry_size) {
        throw std::runtime_error(message);
    }
    return offset + count * entry_size;
}

}  // namespace

GmsImage GmsImage::parse(PackedResource resource) {
    GmsImage result;
    result.resource_ = std::move(resource);
    const auto payload = result.resource_.payload();
    if (payload.size() < fixed_header_size) {
        throw std::runtime_error("GMS image is missing its fixed header");
    }
    const ByteReader reader(payload);
    const auto directory_offset = static_cast<std::size_t>(reader.u32(0));
    const auto identifier_table_offset = static_cast<std::size_t>(reader.u32(4));
    if ((directory_offset & 3U) != 0U || (identifier_table_offset & 3U) != 0U ||
        directory_offset > payload.size() - sizeof(std::uint32_t) ||
        identifier_table_offset > payload.size() - sizeof(std::uint32_t) ||
        reader.u32(12) != 4U) {
        throw std::runtime_error("invalid GMS image header");
    }

    const auto directory_count = static_cast<std::size_t>(reader.u32(directory_offset));
    const auto directory_start = directory_offset + sizeof(std::uint32_t);
    static_cast<void>(checked_table_end(
        directory_start,
        directory_count,
        directory_entry_size,
        payload.size(),
        "GMS object-source directory exceeds the decoded image"
    ));
    result.directory_.reserve(directory_count);
    for (std::size_t index = 0; index < directory_count; ++index) {
        const auto entry_offset = directory_start + index * directory_entry_size;
        const auto packed_record = reader.u32(entry_offset);
        const auto record_word_offset = packed_record & record_word_offset_mask;
        const auto record_offset = static_cast<std::size_t>(record_word_offset) * 4U;
        if (record_offset > payload.size() ||
            minimum_record_size > payload.size() - record_offset) {
            throw std::runtime_error("GMS directory references a truncated object source");
        }
        result.directory_.push_back({
            .packed_record_reference = packed_record,
            .auxiliary_value = reader.u32(entry_offset + sizeof(std::uint32_t)),
            .record_offset = static_cast<std::uint32_t>(record_offset),
            .hierarchy_depth = static_cast<std::uint8_t>(packed_record >> 25U),
            .flagged = (packed_record & record_flag) != 0U,
        });
    }

    result.identifier_count_ = reader.u32(identifier_table_offset);
    const auto identifier_table_start = identifier_table_offset + sizeof(std::uint32_t);
    static_cast<void>(checked_table_end(
        identifier_table_start,
        result.identifier_count_,
        sizeof(std::uint32_t),
        payload.size(),
        "GMS identifier table exceeds the decoded image"
    ));
    for (std::size_t index = 0; index < result.identifier_count_; ++index) {
        const auto string_offset = static_cast<std::size_t>(
            reader.u32(identifier_table_start + index * sizeof(std::uint32_t))
        );
        if (string_offset >= payload.size() ||
            std::find(payload.begin() + static_cast<std::ptrdiff_t>(string_offset),
                      payload.end(), std::byte{0}) == payload.end()) {
            throw std::runtime_error("GMS identifier is not NUL-terminated");
        }
    }
    return result;
}

GmsObjectHandle GmsImage::decode_object_handle(std::uint32_t packed_reference) {
    if ((packed_reference & reference_flag_mask) != gms_reference_flag) {
        throw std::runtime_error("invalid GMS object-handle tag");
    }
    const auto offset = packed_reference & reference_offset_mask;
    if (offset % object_slot_size != 0U) {
        throw std::runtime_error("misaligned GMS object handle");
    }
    return {
        .byte_offset = offset,
        .slot_index = static_cast<std::uint32_t>(offset / object_slot_size),
    };
}

}  // namespace off::data
