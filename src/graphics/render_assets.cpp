#include "off/graphics/render_assets.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace off::graphics {

RenderAssetBindings RenderAssetBindings::build(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures
) {
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    std::array<std::size_t, 2048> texture_indexes;
    texture_indexes.fill(missing);
    for (std::size_t index = 0; index < textures.size(); ++index) {
        const auto id = textures[index].id;
        if (id >= texture_indexes.size() || texture_indexes[id] != missing) {
            throw std::runtime_error("render assets contain a duplicate or invalid texture ID");
        }
        texture_indexes[id] = index;
    }

    RenderAssetBindings result;
    result.primitives_.reserve(primitives.size());
    for (std::size_t index = 0; index < primitives.size(); ++index) {
        const auto& primitive = primitives[index];
        if (primitive.flagged_reference) {
            continue;
        }
        PrimitiveTextureBinding binding{
            .primitive_entry_index = index,
            .texture_image_index = std::nullopt,
            .texture_selector_flagged = primitive.texture_selector_flagged,
        };
        if (primitive.texture_id.has_value()) {
            const auto texture_index = texture_indexes[*primitive.texture_id];
            if (texture_index == missing) {
                throw std::runtime_error("render primitive references a missing texture image");
            }
            binding.texture_image_index = texture_index;
        }
        result.primitives_.push_back(binding);
    }
    return result;
}

}  // namespace off::graphics
