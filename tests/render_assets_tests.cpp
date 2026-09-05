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
    primitives[1].flagged_reference = true;

    const auto bindings = off::graphics::RenderAssetBindings::build(primitives, textures);
    check(bindings.primitives().size() == 2, "omit flagged PRM references");
    check(bindings.primitives()[0].primitive_entry_index == 0 &&
              bindings.primitives()[0].texture_image_index == 1 &&
              bindings.primitives()[0].texture_selector_flagged,
          "resolve a primitive texture ID to the TEX image index");
    check(bindings.primitives()[1].primitive_entry_index == 2 &&
              !bindings.primitives()[1].texture_image_index.has_value(),
          "preserve an untextured primitive");

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
