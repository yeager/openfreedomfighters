#include "off/data/install.hpp"
#include "off/graphics/render_preview.hpp"
#include "off/mode.hpp"
#include "off/platform/sdl_gpu_runtime.hpp"

#include <charconv>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void usage(std::ostream &output) {
  output << "Usage: openfreedomfighters --data PATH [--mode original|modern] "
            "[--verify-only] [--frame-limit COUNT] [--show-graphics-menu] "
            "[--screenshot FILE.bmp]\n";
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path data_path;
  auto mode = off::Mode::original;
  bool verify_only = false;
  std::size_t frame_limit = 0;
  bool show_graphics_menu = false;
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
  if (data_path.empty()) {
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

  const auto verification = off::data::verify_install(data_path);
  if (!verification) {
    std::cerr << "Game-data verification failed: " << verification.message
              << '\n';
    return 3;
  }
  std::cout << verification.message << '\n'
            << "Mode: " << off::mode_name(mode) << '\n';
  if (verify_only) {
    return 0;
  }
  off::graphics::RenderPreviewAsset preview;
  try {
    preview = off::graphics::load_startup_render_preview(data_path);
  } catch (const std::exception &error) {
    std::cerr << "Render-preview loading failed: " << error.what() << '\n';
    return 4;
  }
  const auto runtime = off::platform::run_sdl_gpu_runtime(
      mode, preview, frame_limit, show_graphics_menu, screenshot_path);
  if (!runtime.success) {
    std::cerr << "Native runtime failed: " << runtime.message << '\n';
    return 4;
  }
  std::cout << runtime.message << '\n';
  return 0;
}
