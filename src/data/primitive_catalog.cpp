#include "off/data/primitive_catalog.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::data {
namespace {

constexpr std::size_t header_size = 16;
constexpr std::size_t descriptor_size = 124;
constexpr std::size_t vertex_size = 36;
constexpr std::size_t maximum_file_size = 256U * 1024U * 1024U;
constexpr std::uint32_t maximum_entries = 65'536;
constexpr std::uint32_t maximum_vertex_count = 16'384;
constexpr std::uint32_t maximum_topology_words = 1U * 1024U * 1024U;
constexpr std::uint32_t reference_flag = 0x80000000U;
constexpr std::uint32_t offset_mask = 0x7fffffffU;

void require_data_offset(
    std::uint32_t value,
    std::size_t descriptor_offset,
    bool optional
) {
    if (optional && value == 0) {
        return;
    }
    if (value < header_size || value >= descriptor_offset || (value & 1U) != 0U) {
        throw std::runtime_error("primitive descriptor contains an invalid data offset");
    }
}

[[nodiscard]] float read_float(const ByteReader& reader, std::size_t offset) {
    const auto value = std::bit_cast<float>(reader.u32(offset));
    if (!std::isfinite(value)) {
        throw std::runtime_error("primitive vertex contains a non-finite component");
    }
    return value;
}

}  // namespace

PrimitiveCatalog PrimitiveCatalog::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size + descriptor_size + sizeof(std::uint32_t) ||
        bytes.size() > maximum_file_size) {
        throw std::runtime_error("invalid primitive-catalog file size");
    }
    const ByteReader reader(bytes);
    const auto base_data_end = reader.u32(0);
    const auto index_offset = reader.u32(4);
    const auto repeated_index_offset = reader.u32(8);
    const auto entry_count = reader.u32(12);
    const auto index_bytes = static_cast<std::uint64_t>(entry_count) * 4U;
    if (entry_count == 0 || entry_count > maximum_entries ||
        index_offset != repeated_index_offset || base_data_end < header_size ||
        base_data_end > index_offset || index_offset > bytes.size() ||
        (base_data_end & 1U) != 0U || (index_offset & 1U) != 0U ||
        static_cast<std::uint64_t>(index_offset) + index_bytes != bytes.size()) {
        throw std::runtime_error("invalid primitive-catalog envelope");
    }

    PrimitiveCatalog result;
    result.base_data_end_ = base_data_end;
    result.entries_.reserve(entry_count);
    std::vector<std::uint32_t> descriptor_offsets;
    descriptor_offsets.reserve(entry_count);

    for (std::uint32_t entry_index = 0; entry_index < entry_count; ++entry_index) {
        const auto packed_index = reader.u32(
            static_cast<std::size_t>(index_offset) + entry_index * 4U
        );
        const auto descriptor_offset = packed_index & offset_mask;
        if (descriptor_offset < header_size || (descriptor_offset & 1U) != 0U ||
            descriptor_offset > index_offset ||
            descriptor_size > index_offset - descriptor_offset) {
            throw std::runtime_error("primitive index contains an invalid descriptor offset");
        }
        descriptor_offsets.push_back(descriptor_offset);

        PrimitiveEntry entry;
        entry.packed_index = packed_index;
        entry.descriptor_offset = descriptor_offset;
        entry.flagged_reference = (packed_index & reference_flag) != 0;
        if (entry.flagged_reference) {
            result.entries_.push_back(entry);
            continue;
        }

        entry.format_flags = reader.u16(descriptor_offset);
        entry.primitive_kind = reader.u16(descriptor_offset + 2U);
        entry.secondary_data_offset = reader.u32(descriptor_offset + 8U);
        entry.vertex_count = reader.u16(descriptor_offset + 14U);
        entry.auxiliary_data_offset = reader.u32(descriptor_offset + 16U);
        entry.vertex_data_offset = reader.u32(descriptor_offset + 20U);
        entry.topology_data_offset = reader.u32(descriptor_offset + 60U);
        const auto topology_word_count = reader.u32(descriptor_offset + 64U);

        if ((entry.primitive_kind != 0 && entry.primitive_kind != 3) ||
            entry.vertex_count == 0 || entry.vertex_count > maximum_vertex_count ||
            topology_word_count == 0 || topology_word_count > maximum_topology_words) {
            throw std::runtime_error("primitive descriptor contains unsupported counts or kind");
        }
        require_data_offset(entry.secondary_data_offset, descriptor_offset, true);
        require_data_offset(entry.auxiliary_data_offset, descriptor_offset, false);
        require_data_offset(entry.vertex_data_offset, descriptor_offset, false);
        require_data_offset(entry.topology_data_offset, descriptor_offset, false);

        const auto vertex_bytes =
            static_cast<std::uint64_t>(entry.vertex_count) * vertex_size;
        if (static_cast<std::uint64_t>(entry.vertex_data_offset) + vertex_bytes >
            descriptor_offset) {
            throw std::runtime_error("primitive vertex table exceeds descriptor bounds");
        }
        const auto topology_bytes =
            static_cast<std::uint64_t>(topology_word_count) * sizeof(std::uint16_t);
        if (static_cast<std::uint64_t>(entry.topology_data_offset) + topology_bytes >
            descriptor_offset) {
            throw std::runtime_error("primitive topology exceeds descriptor bounds");
        }

        entry.vertices.reserve(entry.vertex_count);
        for (std::uint32_t vertex_index = 0; vertex_index < entry.vertex_count;
             ++vertex_index) {
            const auto vertex_offset = entry.vertex_data_offset +
                                       static_cast<std::size_t>(vertex_index) * vertex_size;
            const auto packed_color = reader.u32(vertex_offset + 24U);
            entry.vertices.push_back({
                .position = {
                    read_float(reader, vertex_offset),
                    read_float(reader, vertex_offset + 4U),
                    read_float(reader, vertex_offset + 8U),
                },
                .normal = {
                    read_float(reader, vertex_offset + 12U),
                    read_float(reader, vertex_offset + 16U),
                    read_float(reader, vertex_offset + 20U),
                },
                .color_rgba = {
                    static_cast<std::uint8_t>((packed_color >> 16U) & 0xffU),
                    static_cast<std::uint8_t>((packed_color >> 8U) & 0xffU),
                    static_cast<std::uint8_t>(packed_color & 0xffU),
                    static_cast<std::uint8_t>(packed_color >> 24U),
                },
                .texture_coordinates = {
                    read_float(reader, vertex_offset + 28U),
                    read_float(reader, vertex_offset + 32U),
                },
            });
        }

        const auto batch_count = reader.u16(entry.topology_data_offset);
        entry.batches.reserve(batch_count);
        std::uint32_t word_cursor = 1;
        for (std::uint32_t batch_index = 0; batch_index < batch_count; ++batch_index) {
            if (word_cursor >= topology_word_count) {
                throw std::runtime_error("primitive topology is missing an index count");
            }
            const auto index_count = reader.u16(
                entry.topology_data_offset + static_cast<std::size_t>(word_cursor) * 2U
            );
            ++word_cursor;
            if (index_count == 0 || index_count > topology_word_count - word_cursor) {
                throw std::runtime_error("primitive topology batch exceeds its word table");
            }
            PrimitiveBatch batch;
            batch.indices.reserve(index_count);
            for (std::uint32_t index = 0; index < index_count; ++index) {
                const auto vertex_index = reader.u16(
                    entry.topology_data_offset +
                    static_cast<std::size_t>(word_cursor + index) * 2U
                );
                if (vertex_index >= entry.vertex_count) {
                    throw std::runtime_error("primitive topology references a missing vertex");
                }
                batch.indices.push_back(vertex_index);
            }
            word_cursor += index_count;
            entry.batches.push_back(std::move(batch));
        }
        if (word_cursor != topology_word_count) {
            throw std::runtime_error("primitive topology contains trailing words");
        }
        result.entries_.push_back(std::move(entry));
    }
    std::ranges::sort(descriptor_offsets);
    if (std::adjacent_find(
            descriptor_offsets.begin(),
            descriptor_offsets.end(),
            [](std::uint32_t first, std::uint32_t second) {
                return second - first < descriptor_size;
            }
        ) != descriptor_offsets.end()) {
        throw std::runtime_error("primitive index contains overlapping descriptors");
    }
    return result;
}

}  // namespace off::data
