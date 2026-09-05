#include "off/data/render_map.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace off::data {
namespace {

constexpr std::size_t header_size = 32;
constexpr std::size_t index_entry_size = 16;
constexpr std::size_t descriptor_size = 84;
constexpr std::size_t bytes_per_entry = index_entry_size + descriptor_size;
constexpr std::size_t maximum_file_size = 16U * 1024U * 1024U;
constexpr std::size_t maximum_entries = 65'536;
constexpr std::uint32_t supported_quantization_scale = 0x00010000U;

[[nodiscard]] float read_finite_float(const ByteReader& reader, std::size_t offset) {
    const auto value = std::bit_cast<float>(reader.u32(offset));
    if (!std::isfinite(value)) {
        throw std::runtime_error("render-map descriptor contains a non-finite value");
    }
    return value;
}

template <std::size_t Size>
[[nodiscard]] std::array<float, Size> read_float_array(
    const ByteReader& reader,
    std::size_t offset
) {
    std::array<float, Size> values{};
    for (std::size_t index = 0; index < Size; ++index) {
        values[index] = read_finite_float(reader, offset + index * sizeof(float));
    }
    return values;
}

}  // namespace

RenderMap RenderMap::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size + bytes_per_entry || bytes.size() > maximum_file_size) {
        throw std::runtime_error("invalid render-map file size");
    }
    const ByteReader reader(bytes);
    const auto index_offset = reader.u32(0);
    if (index_offset < header_size || index_offset > bytes.size() ||
        (index_offset & 0x0fU) != 0U ||
        (bytes.size() - index_offset) % bytes_per_entry != 0U) {
        throw std::runtime_error("invalid render-map envelope");
    }
    const auto entry_count = (bytes.size() - index_offset) / bytes_per_entry;
    if (entry_count == 0 || entry_count > maximum_entries) {
        throw std::runtime_error("invalid render-map entry count");
    }
    const auto descriptor_start = index_offset + entry_count * index_entry_size;

    RenderMap result;
    result.index_offset_ = index_offset;
    result.root_parameters_ = read_float_array<4>(reader, 4);
    result.quantization_scale_ = reader.u32(20);
    result.hierarchy_flags_ = reader.u32(24);
    result.hierarchy_parameter_ = reader.u32(28);
    if (result.quantization_scale_ != supported_quantization_scale) {
        throw std::runtime_error("unsupported render-map quantization scale");
    }
    result.packed_hierarchy_.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(header_size),
        bytes.begin() + static_cast<std::ptrdiff_t>(index_offset)
    );
    result.entries_.reserve(entry_count);
    std::vector<bool> referenced_descriptors(entry_count, false);

    for (std::size_t entry_index = 0; entry_index < entry_count; ++entry_index) {
        const auto index_entry_offset = index_offset + entry_index * index_entry_size;
        const auto descriptor_offset = reader.u32(index_entry_offset);
        if (descriptor_offset < descriptor_start || descriptor_offset > bytes.size() ||
            descriptor_size > bytes.size() - descriptor_offset ||
            (descriptor_offset - descriptor_start) % descriptor_size != 0U) {
            throw std::runtime_error("render-map index contains an invalid descriptor offset");
        }
        const auto descriptor_index =
            (descriptor_offset - descriptor_start) / descriptor_size;
        if (descriptor_index >= entry_count || referenced_descriptors[descriptor_index]) {
            throw std::runtime_error("render-map index does not uniquely cover its descriptors");
        }
        referenced_descriptors[descriptor_index] = true;

        RenderMapEntry entry;
        entry.descriptor_offset = descriptor_offset;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            entry.bounds.minimum[axis] = reader.u16(index_entry_offset + 4U + axis * 2U);
            entry.bounds.maximum[axis] = reader.u16(index_entry_offset + 10U + axis * 2U);
            if (entry.bounds.minimum[axis] > entry.bounds.maximum[axis]) {
                throw std::runtime_error("render-map index contains inverted bounds");
            }
        }

        entry.object.kind = reader.u32(descriptor_offset);
        if (entry.object.kind > 1U) {
            throw std::runtime_error("render-map descriptor contains an unsupported kind");
        }
        entry.object.orientation = read_float_array<9>(reader, descriptor_offset + 4U);
        entry.object.position = read_float_array<3>(reader, descriptor_offset + 40U);
        entry.object.auxiliary_position =
            read_float_array<3>(reader, descriptor_offset + 52U);
        entry.object.extents = read_float_array<3>(reader, descriptor_offset + 64U);
        entry.object.scale_terms = read_float_array<2>(reader, descriptor_offset + 76U);
        result.entries_.push_back(entry);
    }
    if (!std::ranges::all_of(referenced_descriptors, [](bool value) { return value; })) {
        throw std::runtime_error("render-map contains an unreferenced descriptor");
    }
    return result;
}

}  // namespace off::data
