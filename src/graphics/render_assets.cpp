#include "off/graphics/render_assets.hpp"

#include <algorithm>
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
        const auto [minimum_alpha, maximum_alpha] = std::minmax_element(
            primitive.vertices.begin(),
            primitive.vertices.end(),
            [](const auto& left, const auto& right) {
                return left.color_rgba[3] < right.color_rgba[3];
            }
        );
        if (minimum_alpha == primitive.vertices.end()) {
            throw std::runtime_error("render primitive has no vertices");
        }
        const auto minimum_vertex_alpha = minimum_alpha->color_rgba[3];
        const auto maximum_vertex_alpha = maximum_alpha->color_rgba[3];
        auto vertex_alpha_class = VertexAlphaClass::variable;
        if (minimum_vertex_alpha == 255 && maximum_vertex_alpha == 255) {
            vertex_alpha_class = VertexAlphaClass::opaque;
        } else if (minimum_vertex_alpha == 0 && maximum_vertex_alpha == 0) {
            vertex_alpha_class = VertexAlphaClass::fully_transparent;
        }
        PrimitiveTextureBinding binding{
            .primitive_entry_index = index,
            .texture_image_index = std::nullopt,
            .texture_selector_flagged = primitive.texture_selector_flagged,
            .vertex_alpha_class = vertex_alpha_class,
            .minimum_vertex_alpha = minimum_vertex_alpha,
            .maximum_vertex_alpha = maximum_vertex_alpha,
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
