#pragma once

#include "off/graphics/scene_gpu_plan.hpp"
#include "off/mode.hpp"
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

[[nodiscard]] RuntimeResult
run_sdl_gpu_runtime(Mode mode, const graphics::SceneGpuPlan &scene,
                    const ui::RetailUiFontSet &ui_fonts,
                    const ui::RetailUiTextureSet &ui_textures,
                    std::size_t frame_limit = 0,
                    bool show_graphics_menu = false,
                    const std::filesystem::path &screenshot_path = {});

} // namespace off::platform
