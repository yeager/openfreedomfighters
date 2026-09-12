#include "off/settings/graphics_settings_store.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
std::string read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

int main() {
  const auto root =
      std::filesystem::current_path() /
      ("off-graphics-store-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = root / "graphics.settings";
  check(off::settings::load_graphics_settings(path).status ==
            off::settings::GraphicsSettingsLoadStatus::missing,
        "distinguish a missing settings file from an I/O failure");
  check(off::settings::load_graphics_settings(root).status ==
            off::settings::GraphicsSettingsLoadStatus::io_error,
        "report an unreadable directory as an I/O failure");
#ifndef _WIN32
  {
    const auto target = root / "linked-settings-target";
    const auto linked = root / "linked.settings";
    {
      std::ofstream output(target, std::ios::binary);
      output << "do not follow settings links";
    }
    std::error_code link_error;
    std::filesystem::create_symlink(target.filename(), linked, link_error);
    check(!link_error,
          "create a leaf link for settings-link integrity coverage");
    if (!link_error) {
      check(off::settings::load_graphics_settings(linked).status ==
                off::settings::GraphicsSettingsLoadStatus::io_error,
            "never follow a linked settings document while loading");
      check(!off::settings::save_graphics_settings(
                linked, off::settings::RequestedGraphicsSettings{}),
            "never replace a linked settings document while saving");
      check(read(target) == "do not follow settings links",
            "linked settings operations leave the link target untouched");
    }
  }
#endif
  {
    std::ofstream collision(root / "graphics.settings.tmp-0", std::ios::binary);
    collision << "do not replace";
  }
  off::settings::RequestedGraphicsSettings value;
  value.profile = off::Mode::modern;
  value.window_mode = off::settings::WindowMode::borderless_desktop;
  value.windowed_size = {1920, 1080};
  value.present_mode = off::settings::PresentMode::immediate;
  value.modern_plus = true;
  value.render_scale_percent = 125;
  value.upscaler = off::settings::Upscaler::fsr;
  value.shadow_quality = off::settings::ShadowQuality::ultra;
  check(off::settings::save_graphics_settings(path, value),
        "save requested settings atomically");
  check(read(root / "graphics.settings.tmp-0") == "do not replace",
        "exclusive temporary creation does not overwrite a colliding sibling");
  const auto loaded = off::settings::load_graphics_settings(path);
  check(loaded.status == off::settings::GraphicsSettingsLoadStatus::loaded &&
            loaded.settings == value,
        "round trip every requested field");
  const auto persisted_initial =
      off::settings::load_initial_graphics_settings(path, off::Mode::original, false);
  check(persisted_initial == value,
        "valid stored settings supply the initial graphics request");
  const auto command_line_initial =
      off::settings::load_initial_graphics_settings(path, off::Mode::original, true);
  check(command_line_initial.profile == off::Mode::original &&
            command_line_initial.windowed_size == value.windowed_size &&
            command_line_initial.upscaler == value.upscaler,
        "an explicit command-line mode overrides only the saved profile");
  const auto serialized = read(path);
  check(serialized.find("fallback") == std::string::npos &&
            serialized.find("capabilit") == std::string::npos,
        "never serialize effective settings or hardware claims");
  {
    std::ofstream malformed(path, std::ios::binary | std::ios::trunc);
    malformed << "off-graphics-settings=1\nprofile=1\nunknown=1\n";
  }
  const auto malformed_before = read(path);
  const auto rejected = off::settings::load_graphics_settings(path);
  check(rejected.status == off::settings::GraphicsSettingsLoadStatus::invalid &&
            !rejected.settings,
        "reject unknown or incomplete documents");
  check(read(path) == malformed_before,
        "loading malformed data never overwrites it");
  check(off::settings::load_initial_graphics_settings(
            path, off::Mode::modern, false) == off::settings::RequestedGraphicsSettings{},
        "malformed preferences fall back to the default request without repair");
  {
    std::ofstream unsupported(path, std::ios::binary | std::ios::trunc);
    unsupported << "off-graphics-settings=2\n";
  }
  check(off::settings::load_graphics_settings(path).status ==
            off::settings::GraphicsSettingsLoadStatus::invalid,
        "reject unsupported schema versions");
  value.render_scale_percent = 49;
  check(!off::settings::save_graphics_settings(path, value),
        "reject invalid settings before touching the file");
  check(read(path) == "off-graphics-settings=2\n",
        "failed save preserves existing data");
  value.render_scale_percent = 125;
  const auto absent_parent_path = root / "absent-parent" / "graphics.settings";
  check(!off::settings::save_graphics_settings(absent_parent_path, value),
        "do not create a fallback preferences directory when its parent is absent");
  check(!std::filesystem::exists(absent_parent_path.parent_path()),
        "failed save under an absent parent leaves no directory or partial file");
  {
    std::ofstream oversized(path, std::ios::binary | std::ios::trunc);
    oversized << std::string(4097, 'x');
  }
  check(off::settings::load_graphics_settings(path).status ==
            off::settings::GraphicsSettingsLoadStatus::invalid,
        "reject oversized documents without unbounded input");
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  return failures == 0 ? 0 : 1;
}
