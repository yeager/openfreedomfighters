#include "off/data/render_map.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace off::data {
namespace {

constexpr std::size_t fixed_header_size = 20;
constexpr std::size_t minimum_index_offset = 32;
constexpr std::size_t node_size = 6;
constexpr std::size_t index_entry_size = 16;
constexpr std::size_t descriptor_size = 84;
constexpr std::size_t bytes_per_entry = index_entry_size + descriptor_size;
constexpr std::size_t maximum_file_size = 16U * 1024U * 1024U;
constexpr std::size_t maximum_entries = 65'536;
constexpr std::uint32_t maximum_tree_depth = 16;
constexpr std::uint16_t element_count_mask = 0x7ff8U;
constexpr std::uint16_t last_sibling_mask = 0x8000U;
constexpr std::uint16_t octant_mask = 0x0007U;
constexpr std::uint32_t geometry_reference_flag_mask = 0xc0000000U;
constexpr std::uint32_t geometry_reference_flag = 0x40000000U;
constexpr std::uint32_t geometry_reference_value_mask = 0x3fffffffU;
constexpr std::int32_t quantized_root_center = 0x8000;
constexpr std::int32_t quantized_root_size = 0x10000;

[[nodiscard]] float read_finite_float(const ByteReader& reader, std::size_t offset) {
    const auto value = std::bit_cast<float>(reader.u32(offset));
    if (!std::isfinite(value)) {
        throw std::runtime_error("render-map descriptor contains a non-finite value");
    }
    return value;
}

[[nodiscard]] std::int32_t quantize(float value, float center, float factor) {
    const auto transformed = (value - center) * factor + 0.5F;
    const auto wide = static_cast<double>(transformed);
    if (wide <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) {
        return std::numeric_limits<std::int32_t>::min();
    }
    if (wide >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return std::numeric_limits<std::int32_t>::max();
    }
    return static_cast<std::int32_t>(transformed);
}

[[nodiscard]] bool overlaps(
    const std::array<std::int32_t, 3>& minimum,
    const std::array<std::int32_t, 3>& maximum,
    const QuantizedBounds& candidate
) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (minimum[axis] >= candidate.maximum[axis] ||
            maximum[axis] <= candidate.minimum[axis]) {
            return false;
        }
    }
    return true;
}

void require_geometry_reference(std::uint32_t value, bool optional) {
    if (optional && value == 0) {
        return;
    }
    if ((value & geometry_reference_flag_mask) != geometry_reference_flag ||
        (value & geometry_reference_value_mask) % 16U != 0U) {
        throw std::runtime_error("render-map descriptor contains an invalid geometry reference");
    }
}

[[nodiscard]] RenderMapNode read_node(const ByteReader& reader, std::size_t index) {
    const auto offset = fixed_header_size + index * node_size;
    const auto flags = reader.u16(offset);
    return {
        .raw_flags = flags,
        .octant = static_cast<std::uint8_t>(flags & octant_mask),
        .last_sibling = (flags & last_sibling_mask) != 0U,
        .child_index = reader.u16(offset + 2U),
        .element_offset = reader.u16(offset + 4U),
        .element_count = static_cast<std::uint16_t>(
            (flags & element_count_mask) >> 3U
        ),
    };
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
    if (bytes.size() < minimum_index_offset + bytes_per_entry ||
        bytes.size() > maximum_file_size) {
        throw std::runtime_error("invalid render-map file size");
    }
    const ByteReader reader(bytes);
    const auto index_offset = reader.u32(0);
    if (index_offset < minimum_index_offset || index_offset > bytes.size() ||
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
    result.center_ = read_float_array<3>(reader, 4);
    result.quantization_factor_ = read_finite_float(reader, 16);
    if (result.quantization_factor_ <= 0.0F) {
        throw std::runtime_error("render-map contains an invalid quantization factor");
    }

    const auto node_capacity = (index_offset - fixed_header_size) / node_size;
    if (node_capacity == 0) {
        throw std::runtime_error("render-map does not contain a root node");
    }
    std::vector<bool> visited_nodes(node_capacity, false);
    std::vector<bool> referenced_elements(entry_count, false);
    struct PendingParseNode {
        std::size_t index;
        std::uint32_t depth;
    };
    std::vector<PendingParseNode> pending_nodes{{.index = 0, .depth = 0}};
    std::size_t highest_node_index = 0;
    while (!pending_nodes.empty()) {
        const auto current = pending_nodes.back();
        pending_nodes.pop_back();
        const auto node_index = current.index;
        if (node_index >= node_capacity || visited_nodes[node_index]) {
            throw std::runtime_error("render-map hierarchy contains a cycle or reused node");
        }
        visited_nodes[node_index] = true;
        highest_node_index = std::max(highest_node_index, node_index);
        const auto node = read_node(reader, node_index);
        const auto element_end =
            static_cast<std::size_t>(node.element_offset) + node.element_count;
        if (element_end > entry_count) {
            throw std::runtime_error("render-map node references elements outside the index");
        }
        for (std::size_t element = node.element_offset; element < element_end; ++element) {
            if (referenced_elements[element]) {
                throw std::runtime_error("render-map element is referenced by multiple nodes");
            }
            referenced_elements[element] = true;
        }
        if (node.child_index == 0) {
            continue;
        }
        if (current.depth == maximum_tree_depth) {
            throw std::runtime_error("render-map hierarchy exceeds quantized tree depth");
        }
        std::array<bool, 8> occupied_octants{};
        auto child_index = static_cast<std::size_t>(node.child_index);
        while (true) {
            if (child_index >= node_capacity) {
                throw std::runtime_error("render-map child list exceeds the node region");
            }
            const auto child = read_node(reader, child_index);
            if (occupied_octants[child.octant]) {
                throw std::runtime_error("render-map child list repeats an octant");
            }
            occupied_octants[child.octant] = true;
            pending_nodes.push_back({
                .index = child_index,
                .depth = current.depth + 1U,
            });
            if (child.last_sibling) {
                break;
            }
            ++child_index;
        }
    }
    const auto node_count = highest_node_index + 1U;
    if (!std::ranges::all_of(
            visited_nodes.begin(),
            visited_nodes.begin() + static_cast<std::ptrdiff_t>(node_count),
            [](bool value) { return value; }
        )) {
        throw std::runtime_error("render-map hierarchy contains an unreachable node");
    }
    if (!std::ranges::all_of(referenced_elements, [](bool value) { return value; })) {
        throw std::runtime_error("render-map hierarchy does not cover every element");
    }
    const auto hierarchy_end = fixed_header_size + node_count * node_size;
    if (index_offset - hierarchy_end >= 16U) {
        throw std::runtime_error("render-map hierarchy contains excessive trailing data");
    }
    result.nodes_.reserve(node_count);
    for (std::size_t node_index = 0; node_index < node_count; ++node_index) {
        result.nodes_.push_back(read_node(reader, node_index));
    }
    result.alignment_padding_.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(hierarchy_end),
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
        entry.object.primary_geometry_reference = reader.u32(descriptor_offset + 76U);
        entry.object.secondary_geometry_reference = reader.u32(descriptor_offset + 80U);
        require_geometry_reference(entry.object.primary_geometry_reference, false);
        require_geometry_reference(entry.object.secondary_geometry_reference, true);
        result.entries_.push_back(entry);
    }
    if (!std::ranges::all_of(referenced_descriptors, [](bool value) { return value; })) {
        throw std::runtime_error("render-map contains an unreferenced descriptor");
    }
    return result;
}

std::vector<std::size_t> RenderMap::query_bounds(const WorldBounds& bounds) const {
    std::array<std::int32_t, 3> query_minimum{};
    std::array<std::int32_t, 3> query_maximum{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(bounds.minimum[axis]) ||
            !std::isfinite(bounds.maximum[axis]) ||
            bounds.minimum[axis] > bounds.maximum[axis]) {
            throw std::invalid_argument("invalid world-space render-map bounds");
        }
        query_minimum[axis] = quantize(
            bounds.minimum[axis],
            center_[axis],
            quantization_factor_
        );
        const auto maximum = quantize(
            bounds.maximum[axis],
            center_[axis],
            quantization_factor_
        );
        query_maximum[axis] = maximum == std::numeric_limits<std::int32_t>::max()
                                  ? maximum
                                  : maximum + 1;
    }

    struct PendingNode {
        std::size_t index;
        std::uint32_t depth;
        std::array<std::int32_t, 3> center;
    };
    std::vector<PendingNode> pending{{
        .index = 0,
        .depth = 0,
        .center = {
            quantized_root_center,
            quantized_root_center,
            quantized_root_center,
        },
    }};
    std::vector<std::size_t> matches;
    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        const auto& node = nodes_[current.index];
        const auto element_end =
            static_cast<std::size_t>(node.element_offset) + node.element_count;
        for (std::size_t element = node.element_offset; element < element_end; ++element) {
            if (overlaps(query_minimum, query_maximum, entries_[element].bounds)) {
                matches.push_back(element);
            }
        }
        if (node.child_index == 0) {
            continue;
        }

        const auto cell_size = quantized_root_size >> (current.depth + 1U);
        const auto center_offset = cell_size / 2;
        std::vector<PendingNode> children;
        auto child_index = static_cast<std::size_t>(node.child_index);
        while (true) {
            const auto& child = nodes_[child_index];
            auto child_center = current.center;
            bool intersects = true;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const auto positive_octant =
                    (child.octant & (1U << axis)) != 0U;
                child_center[axis] += positive_octant ? center_offset : -center_offset;
                const auto loose_minimum = child_center[axis] - cell_size;
                const auto loose_maximum = child_center[axis] + cell_size;
                if (query_minimum[axis] >= loose_maximum ||
                    query_maximum[axis] < loose_minimum) {
                    intersects = false;
                }
            }
            if (intersects) {
                children.push_back({
                    .index = child_index,
                    .depth = current.depth + 1U,
                    .center = child_center,
                });
            }
            if (child.last_sibling) {
                break;
            }
            ++child_index;
        }
        for (auto child = children.rbegin(); child != children.rend(); ++child) {
            pending.push_back(*child);
        }
    }
    return matches;
}

}  // namespace off::data
