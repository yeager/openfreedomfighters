#pragma once

#include "off/data/primitive_catalog.hpp"
#include "off/data/texture_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

enum class VertexAlphaClass {
    fully_transparent,
    variable,
    opaque,
};

struct PrimitiveTextureBinding {
    std::size_t primitive_entry_index{0};
    std::optional<std::size_t> texture_image_index;
    bool texture_selector_flagged{false};
    VertexAlphaClass vertex_alpha_class{VertexAlphaClass::opaque};
    std::uint8_t minimum_vertex_alpha{255};
    std::uint8_t maximum_vertex_alpha{255};
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
