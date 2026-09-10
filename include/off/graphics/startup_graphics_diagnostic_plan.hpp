#pragma once

#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/startup_graphics_asset.hpp"
#include "off/graphics/startup_graphics_expanded_plan.hpp"

namespace off::graphics {

// A source-image/source-quad diagnostic bridge. Its projection is deliberately
// the generic SceneGpuPlan fit projection, so it is not an original startup
// camera, material pass, or faithful menu presentation.
[[nodiscard]] SceneGpuPlan make_startup_graphics_diagnostic_plan(
    const StartupGraphicsAsset &asset,
    const StartupGraphicsExpandedPlan &expanded);

} // namespace off::graphics
