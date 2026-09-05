#pragma once

#include "off/data/picture_resource.hpp"
#include "off/data/texture_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

inline constexpr std::uint16_t texture_manager_slot_count = 2048;

enum class TextureManagerKeyBank { direct, upper };

struct PictureTextureBinding {
    std::uint32_t prm_offset{0};
    std::uint16_t manager_key{0};
    std::uint16_t texture_id{0};
    std::size_t image_index{0};
    TextureManagerKeyBank bank{TextureManagerKeyBank::direct};
};

class PictureTextureBindings final {
public:
    [[nodiscard]] static PictureTextureBindings build(
        std::span<const PictureTextureResource> resources,
        const TextureCatalog& catalog,
        bool require_upper_bank = false
    );

    [[nodiscard]] std::span<const PictureTextureBinding> entries() const noexcept {
        return entries_;
    }

private:
    std::vector<PictureTextureBinding> entries_;
};

struct PictureQuad {
    float local_x_min{0.0F};
    float local_x_max{0.0F};
    float local_y_min{0.0F};
    float local_y_max{0.0F};
    float local_z{0.0F};
    float u_min{0.0F};
    float u_max{0.0F};
    float v_max{0.0F};
    float v_min{0.0F};
    std::uint32_t modulation_color{0};
    std::size_t descriptor_index{0};
};

struct BoundPictureDrawGroup {
    PictureTextureBinding texture{};
    std::vector<PictureQuad> quads;
};

class PictureDrawPlan final {
public:
    [[nodiscard]] static PictureDrawPlan build(
        const PictureResource& picture,
        const PictureTextureBindings& textures
    );
    [[nodiscard]] static PictureDrawPlan build(
        std::span<const PictureResourceDescriptor> descriptors,
        std::span<const PictureDrawGroup> groups,
        std::span<const PictureTextureBinding> textures
    );

    [[nodiscard]] std::span<const BoundPictureDrawGroup> groups() const noexcept {
        return groups_;
    }

private:
    std::vector<BoundPictureDrawGroup> groups_;
};

}  // namespace off::data
