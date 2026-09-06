#pragma once

#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/startup_graphics_asset.hpp"
#include "off/mode.hpp"
#include "off/platform/sdl_startup.hpp"
#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/retail_ui_textures.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace off::platform {

struct RuntimeResult {
  bool success{false};
  std::string message;
};

// Borrows the successful preflight window. The caller retains ownership until
// this call returns; GPU resources are released before the window is destroyed.
// A null scene is normal startup: no world uploads, depth target, diagnostic
// projection or scene draws. Non-null opts into the separate diagnostic path.
[[nodiscard]] RuntimeResult
run_sdl_gpu_runtime(const StartupWindow &startup_window, Mode mode,
                    const graphics::SceneGpuPlan *scene,
                    const graphics::StartupGraphicsAsset &startup_graphics,
                    const ui::RetailUiFontSet &ui_fonts,
                    const ui::RetailUiTextureSet &ui_textures,
                    std::size_t frame_limit = 0,
                    bool show_graphics_menu = false,
                    const std::filesystem::path &screenshot_path = {});

} // namespace off::platform
