#pragma once

#include "off/data/primitive_catalog.hpp"
#include "off/data/texture_catalog.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

struct PrimitiveTextureBinding {
    std::size_t primitive_entry_index{0};
    std::optional<std::size_t> texture_image_index;
    bool texture_selector_flagged{false};
};

class RenderAssetBindings final {
public:
    [[nodiscard]] static RenderAssetBindings build(
        std::span<const data::PrimitiveEntry> primitives,
        std::span<const data::TextureImage> textures
    );

    [[nodiscard]] std::span<const PrimitiveTextureBinding> primitives() const noexcept {
        return primitives_;
    }

private:
    std::vector<PrimitiveTextureBinding> primitives_;
};

}  // namespace off::graphics
