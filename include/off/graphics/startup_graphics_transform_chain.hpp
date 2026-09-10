#pragma once

#include "off/data/startup_graphics_composition.hpp"
#include "off/graphics/picture_transform.hpp"

#include <span>

namespace off::graphics {

// Composes one root-to-picture affine chain in directory order. This is a
// renderer-neutral source transform: projection, visibility, materials and GPU
// submission remain separate stages.
[[nodiscard]] PictureCacheTransform compose_startup_graphics_transform_chain(
    std::span<const data::StartupGraphicsLocalTransform> chain);

} // namespace off::graphics
