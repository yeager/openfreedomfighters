#pragma once

#include "off/graphics/scene_gpu_plan.hpp"
#include "off/mode.hpp"

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
                    std::size_t frame_limit = 0,
                    bool show_graphics_menu = false,
                    const std::filesystem::path &screenshot_path = {});

} // namespace off::platform
