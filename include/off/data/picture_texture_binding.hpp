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
        std::span<const PictureFrameTextureResource> resources,
        const TextureCatalog& catalog,
        bool require_upper_bank = false
    );

    [[nodiscard]] std::span<const PictureTextureBinding> entries() const noexcept {
        return entries_;
    }

private:
    std::vector<PictureTextureBinding> entries_;
};

}  // namespace off::data
