#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

struct QuantizedBounds {
    std::array<std::uint16_t, 3> minimum{};
    std::array<std::uint16_t, 3> maximum{};
};

struct WorldBounds {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
};

struct RenderMapObject {
    std::uint32_t kind{0};
    std::array<float, 9> orientation{};
    std::array<float, 3> position{};
    std::array<float, 3> auxiliary_position{};
    std::array<float, 3> extents{};
    std::array<float, 2> scale_terms{};
};

struct RenderMapEntry {
    std::uint32_t descriptor_offset{0};
    QuantizedBounds bounds;
    RenderMapObject object;
};

struct RenderMapNode {
    std::uint16_t raw_flags{0};
    std::uint8_t octant{0};
    bool last_sibling{false};
    std::uint16_t child_index{0};
    std::uint16_t element_offset{0};
    std::uint16_t element_count{0};
};

class RenderMap final {
public:
    [[nodiscard]] static RenderMap parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::uint32_t index_offset() const noexcept {
        return index_offset_;
    }
    [[nodiscard]] const std::array<float, 3>& center() const noexcept {
        return center_;
    }
    [[nodiscard]] float quantization_factor() const noexcept {
        return quantization_factor_;
    }
    [[nodiscard]] std::span<const RenderMapNode> nodes() const noexcept {
        return nodes_;
    }
    [[nodiscard]] std::span<const std::byte> alignment_padding() const noexcept {
        return alignment_padding_;
    }
    [[nodiscard]] std::span<const RenderMapEntry> entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::vector<std::size_t> query_bounds(
        const WorldBounds& bounds
    ) const;

private:
    std::uint32_t index_offset_{0};
    std::array<float, 3> center_{};
    float quantization_factor_{0.0F};
    std::vector<RenderMapNode> nodes_;
    std::vector<std::byte> alignment_padding_;
    std::vector<RenderMapEntry> entries_;
};

}  // namespace off::data
