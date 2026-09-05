#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

struct PrimitiveBatch {
    std::vector<std::uint16_t> indices;
};

struct PrimitiveVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<std::uint8_t, 4> color_rgba{};
    std::array<float, 2> texture_coordinates{};
};

struct PrimitiveEntry {
    std::uint32_t packed_index{0};
    std::uint32_t descriptor_offset{0};
    bool flagged_reference{false};
    std::uint16_t format_flags{0};
    std::uint16_t primitive_kind{0};
    std::uint16_t vertex_count{0};
    std::uint32_t secondary_data_offset{0};
    std::uint32_t auxiliary_data_offset{0};
    std::uint32_t vertex_data_offset{0};
    std::uint32_t topology_data_offset{0};
    std::vector<PrimitiveVertex> vertices;
    std::vector<PrimitiveBatch> batches;
};

class PrimitiveCatalog final {
public:
    [[nodiscard]] static PrimitiveCatalog parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::uint32_t base_data_end() const noexcept {
        return base_data_end_;
    }
    [[nodiscard]] std::span<const PrimitiveEntry> entries() const noexcept {
        return entries_;
    }

private:
    std::uint32_t base_data_end_{0};
    std::vector<PrimitiveEntry> entries_;
};

}  // namespace off::data
