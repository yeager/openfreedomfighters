#include "off/graphics/render_assets.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    std::vector<off::data::TextureImage> textures(2);
    textures[0].id = 17;
    textures[1].id = 42;

    std::vector<off::data::PrimitiveEntry> primitives(3);
    primitives[0].texture_id = 42;
    primitives[0].texture_selector_flagged = true;
    primitives[0].vertices.resize(2);
    primitives[0].vertices[0].color_rgba[3] = 48;
    primitives[0].vertices[1].color_rgba[3] = 192;
    primitives[1].flagged_reference = true;
    primitives[2].vertices.resize(1);
    primitives[2].vertices[0].color_rgba[3] = 255;

    const auto bindings = off::graphics::RenderAssetBindings::build(primitives, textures);
    check(bindings.primitives().size() == 2, "omit flagged PRM references");
    check(bindings.primitives()[0].primitive_entry_index == 0 &&
              bindings.primitives()[0].texture_image_index == 1 &&
              bindings.primitives()[0].texture_selector_flagged &&
              bindings.primitives()[0].vertex_alpha_class ==
                  off::graphics::VertexAlphaClass::variable &&
              bindings.primitives()[0].minimum_vertex_alpha == 48 &&
              bindings.primitives()[0].maximum_vertex_alpha == 192,
          "resolve a primitive texture ID to the TEX image index");
    check(bindings.primitives()[1].primitive_entry_index == 2 &&
              !bindings.primitives()[1].texture_image_index.has_value(),
          "preserve an untextured primitive");

    auto missing_vertices = primitives;
    missing_vertices[0].vertices.clear();
    bool missing_vertices_rejected = false;
    try {
        static_cast<void>(
            off::graphics::RenderAssetBindings::build(missing_vertices, textures)
        );
    } catch (const std::runtime_error&) {
        missing_vertices_rejected = true;
    }
    check(missing_vertices_rejected, "reject a primitive without vertices");

    auto missing_texture = primitives;
    missing_texture[0].texture_id = 99;
    bool missing_rejected = false;
    try {
        static_cast<void>(
            off::graphics::RenderAssetBindings::build(missing_texture, textures)
        );
    } catch (const std::runtime_error&) {
        missing_rejected = true;
    }
    check(missing_rejected, "reject a missing texture image");

    auto duplicate_textures = textures;
    duplicate_textures[1].id = 17;
    bool duplicate_rejected = false;
    try {
        static_cast<void>(
            off::graphics::RenderAssetBindings::build(primitives, duplicate_textures)
        );
    } catch (const std::runtime_error&) {
        duplicate_rejected = true;
    }
    check(duplicate_rejected, "reject duplicate texture IDs");

    return failures == 0 ? 0 : 1;
}
