#include "off/data/install.hpp"
#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/scene_render.hpp"
#include "off/graphics/startup_graphics_asset.hpp"
#include "off/mode.hpp"
#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/platform/sdl_startup.hpp"
#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/retail_ui_textures.hpp"

#include <charconv>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace {

void usage(std::ostream &output) {
  output << "Usage: openfreedomfighters --data PATH [--mode original|modern] "
            "[--verify-only] [--frame-limit COUNT] [--show-graphics-menu] "
            "[--screenshot FILE.bmp] [--diagnostic-scene]\n";
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path data_path;
  auto mode = off::Mode::original;
  bool verify_only = false;
  std::size_t frame_limit = 0;
  bool show_graphics_menu = false;
  bool diagnostic_scene = false;
  std::filesystem::path screenshot_path;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--data" && index + 1 < argc) {
      data_path = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      const auto parsed = off::parse_mode(argv[++index]);
      if (!parsed) {
        std::cerr << "Unknown mode. Expected original or modern.\n";
        return 2;
      }
      mode = *parsed;
    } else if (argument == "--verify-only") {
      verify_only = true;
    } else if (argument == "--frame-limit" && index + 1 < argc) {
      const std::string_view value{argv[++index]};
      const auto [end, error] = std::from_chars(
          value.data(), value.data() + value.size(), frame_limit);
      if (error != std::errc{} || end != value.data() + value.size() ||
          frame_limit == 0) {
        std::cerr << "Frame limit must be a positive integer.\n";
        return 2;
      }
    } else if (argument == "--show-graphics-menu") {
      show_graphics_menu = true;
    } else if (argument == "--diagnostic-scene") {
      diagnostic_scene = true;
    } else if (argument == "--screenshot" && index + 1 < argc) {
      screenshot_path = argv[++index];
    } else if (argument == "--help" || argument == "-h") {
      usage(std::cout);
      return 0;
    } else if (argument == "--version") {
      std::cout << "OpenFreedomFighters 0.1.0\n";
      return 0;
    } else {
      std::cerr << "Unknown or incomplete argument: " << argument << '\n';
      usage(std::cerr);
      return 2;
    }
  }
  if (data_path.empty() && verify_only) {
    std::cerr
        << "A legally purchased Freedom Fighters installation is required.\n";
    usage(std::cerr);
    return 2;
  }
  if (!screenshot_path.empty() && screenshot_path.extension() != ".bmp") {
    std::cerr << "Screenshot output must use the .bmp extension.\n";
    return 2;
  }
  if (verify_only && !screenshot_path.empty()) {
    std::cerr << "A screenshot cannot be captured in verify-only mode.\n";
    return 2;
  }
  if (!screenshot_path.empty()) {
    auto temporary = screenshot_path;
    temporary += ".part";
    if (std::filesystem::exists(screenshot_path) ||
        std::filesystem::exists(temporary)) {
      std::cerr
          << "Screenshot output already exists; refusing to overwrite it.\n";
      return 2;
    }
    const auto parent = screenshot_path.has_parent_path()
                            ? screenshot_path.parent_path()
                            : std::filesystem::current_path();
    if (!std::filesystem::is_directory(parent)) {
      std::cerr << "Screenshot output directory does not exist.\n";
      return 2;
    }
  }

  std::optional<off::data::InstallVerification> verification;
  std::optional<off::graphics::SceneGpuPlan> scene;
  std::optional<off::graphics::SceneRenderAsset> startup_ui_scene_resources;
  std::optional<off::graphics::StartupGraphicsAsset> startup_graphics;
  std::optional<off::graphics::IntroPreparedResources> intro_resources;
  off::ui::RetailUiFontSet ui_fonts;
  off::ui::RetailUiTextureSet ui_textures;
  off::platform::StartupWindow startup_window;
  if (!verify_only) {
    auto preflight = off::platform::run_sdl_startup_preflight(data_path, [&] {
      if (diagnostic_scene) {
        scene.emplace(off::graphics::prepare_scene_gpu_plan(
            off::graphics::load_diagnostic_scene_render_asset(data_path)));
      } else {
        // Retain exact UI-archive resources, not an original first-scene
        // selection or a guessed camera/world draw plan.
        startup_ui_scene_resources.emplace(
            off::graphics::load_startup_scene_render_asset(data_path));
        // Prepare authored first-cut resources without admitting a scene or
        // manufacturing lifecycle state. Keep ownership through the runtime.
        intro_resources.emplace(off::graphics::load_intro_prepared_resources(
            data_path / "Scenes" / "FF-Intro.ZIP"));
      }
      startup_graphics.emplace(off::graphics::load_startup_graphics_asset(
          data_path / "Scenes" / "FF-StartUp.ZIP"));
      ui_fonts = off::ui::load_retail_ui_fonts(
          data_path / "Scenes" / "FF-StartUp.ZIP");
    });
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::quit_requested)
      return 0;
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::data_error) {
      std::cerr << "Game-data verification failed: "
                << preflight.verification.message << '\n';
      return 3;
    }
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::platform_error) {
      std::cerr << "Native startup failed: " << preflight.message << '\n';
      return 4;
    }
    verification = preflight.verification;
    startup_window = std::move(preflight.window);
  } else {
    verification = off::data::verify_install(data_path);
  }

  if (!*verification) {
    std::cerr << "Game-data verification failed: " << verification->message
              << '\n';
    return 3;
  }
  std::cout << verification->message << '\n'
            << "Mode: " << off::mode_name(mode) << '\n';
  if (verify_only) {
    return 0;
  }
  if (!diagnostic_scene)
    std::cout << "Authored startup resources loaded; world rendering pending. "
                 "This is not gameplay or a faithful rendered startup menu.\n";
  if (intro_resources)
    std::cout << "Intro first-cut CPU resources retained: "
              << intro_resources->pictures().size() << " pictures, "
              << intro_resources->images().size()
              << " images; lifecycle and rendering not activated.\n";
  const auto runtime = off::platform::run_sdl_gpu_runtime(
      startup_window, mode, scene ? &*scene : nullptr, *startup_graphics,
      ui_fonts, ui_textures,
      frame_limit, show_graphics_menu, screenshot_path);
  if (!runtime.success) {
    std::cerr << "Native runtime failed: " << runtime.message << '\n';
    return 4;
  }
  std::cout << runtime.message << '\n';
  return 0;
}
