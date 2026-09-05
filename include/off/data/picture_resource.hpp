#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

struct PictureResourceDescriptor {
    float local_center_x{0.0F};
    float local_center_y{0.0F};
    float local_z{0.0F};
    float u_min{0.0F};
    float u_max{0.0F};
    float v_max{0.0F};
    float v_min{0.0F};
    float horizontal_edge_span{0.0F};
    float vertical_edge_span{0.0F};
    std::uint32_t modulation_color{0};
    std::array<std::byte, 40> encoded{};
};

struct PictureDrawGroup {
    std::size_t descriptor_span_count{0};
    std::size_t first_descriptor_index{0};
};

struct PictureTextureResource {
    std::uint32_t prm_offset{0};
    std::uint16_t manager_key{0};
    std::array<std::byte, 32> encoded{};
};

class PictureResource final {
public:
    [[nodiscard]] static PictureResource parse(
        std::span<const std::byte> allocation,
        std::uint32_t relocation_key
    );

    [[nodiscard]] std::span<const PictureResourceDescriptor> descriptors() const
        noexcept {
        return descriptors_;
    }
    [[nodiscard]] std::span<const std::uint32_t>
    texture_references() const noexcept {
        return texture_references_;
    }
    [[nodiscard]] std::span<const PictureTextureResource>
    texture_resources() const noexcept {
        return texture_resources_;
    }
    [[nodiscard]] std::span<const PictureDrawGroup> draw_groups() const noexcept {
        return draw_groups_;
    }
    [[nodiscard]] std::size_t encoded_size() const noexcept {
        return encoded_size_;
    }

private:
    std::vector<PictureResourceDescriptor> descriptors_;
    std::vector<std::uint32_t> texture_references_;
    std::vector<PictureTextureResource> texture_resources_;
    std::vector<PictureDrawGroup> draw_groups_;
    std::size_t encoded_size_{0};
};

}  // namespace off::data
