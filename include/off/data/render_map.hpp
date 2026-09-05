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

class RenderMap final {
public:
    [[nodiscard]] static RenderMap parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::uint32_t index_offset() const noexcept {
        return index_offset_;
    }
    [[nodiscard]] const std::array<float, 4>& root_parameters() const noexcept {
        return root_parameters_;
    }
    [[nodiscard]] std::uint32_t quantization_scale() const noexcept {
        return quantization_scale_;
    }
    [[nodiscard]] std::uint32_t hierarchy_flags() const noexcept {
        return hierarchy_flags_;
    }
    [[nodiscard]] std::uint32_t hierarchy_parameter() const noexcept {
        return hierarchy_parameter_;
    }
    [[nodiscard]] std::span<const std::byte> packed_hierarchy() const noexcept {
        return packed_hierarchy_;
    }
    [[nodiscard]] std::span<const RenderMapEntry> entries() const noexcept {
        return entries_;
    }

private:
    std::uint32_t index_offset_{0};
    std::array<float, 4> root_parameters_{};
    std::uint32_t quantization_scale_{0};
    std::uint32_t hierarchy_flags_{0};
    std::uint32_t hierarchy_parameter_{0};
    std::vector<std::byte> packed_hierarchy_;
    std::vector<RenderMapEntry> entries_;
};

}  // namespace off::data
