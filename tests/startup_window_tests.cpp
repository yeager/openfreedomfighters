#include "off/platform/sdl_startup.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << ": " << SDL_GetError() << '\n';
    std::exit(1);
  }
}

double linear_component(unsigned char component) {
  const double normalized = static_cast<double>(component) / 255.0;
  return normalized <= 0.04045 ? normalized / 12.92
                               : std::pow((normalized + 0.055) / 1.055, 2.4);
}

double luminance(off::platform::StartupRgb colour) {
  return 0.2126 * linear_component(colour.red) +
         0.7152 * linear_component(colour.green) +
         0.0722 * linear_component(colour.blue);
}

double contrast_ratio(off::platform::StartupRgb first,
                      off::platform::StartupRgb second) {
  const double high = std::max(luminance(first), luminance(second));
  const double low = std::min(luminance(first), luminance(second));
  return (high + 0.05) / (low + 0.05);
}
} // namespace

int main() {
  using off::platform::StartupWindow;
  static_assert(!std::is_copy_constructible_v<StartupWindow>);
  static_assert(std::is_nothrow_move_constructible_v<StartupWindow>);
  static_assert(!std::is_copy_constructible_v<
                off::platform::StartupPreflightResult>);
  const auto overlay = off::platform::startup_splash_overlay_layout(1280, 720);
  check(overlay.version == std::string_view{"v" OFF_VERSION},
        "splash version comes from the build release version");
  check(overlay.credit == "Daniel Nylander", "splash carries the project credit");
  check(overlay.font_point_size == 26 && overlay.margin == 18,
        "splash overlay selects its shared font and margin policy");
  check(overlay.version_left == 18 && overlay.baseline == 702,
        "version is placed at the lower-left splash margin");
  check(overlay.baseline == 720 - overlay.margin,
        "both overlay baselines use the actual lower margin");
  const auto tiny_overlay = off::platform::startup_splash_overlay_layout(0, 0);
  check(tiny_overlay.margin == 1 && tiny_overlay.font_point_size == 18 &&
            tiny_overlay.version_left == 1 && tiny_overlay.baseline == 0,
        "splash layout remains defined during a transient zero-sized resize");
  const auto error_backdrop =
      off::platform::startup_data_error_backdrop_layout(1280, 720);
  check(error_backdrop.left == 160 && error_backdrop.top == 240 &&
            error_backdrop.width == 960 && error_backdrop.height == 240,
        "data error backing panel is centred at the expected startup size");
  check(error_backdrop.left >= 0 && error_backdrop.top >= 0 &&
            error_backdrop.left + error_backdrop.width <= 1280 &&
            error_backdrop.top + error_backdrop.height <= 720,
        "data error backing panel stays in the startup surface");
  const auto tiny_backdrop =
      off::platform::startup_data_error_backdrop_layout(1, 1);
  check(tiny_backdrop.left == 0 && tiny_backdrop.top == 0 &&
            tiny_backdrop.width == 1 && tiny_backdrop.height == 1,
        "data error backing panel remains in bounds during a zero-scale resize");
  check(contrast_ratio(error_backdrop.border, error_backdrop.panel) >= 3.0,
        "data error border remains distinguishable against its backing panel");
  check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"), "select dummy video");
  check(SDL_Init(SDL_INIT_VIDEO), "initialize video without a display");
  const auto cache_root =
      off::platform::application_deep_audit_cache_root();
  check(!cache_root.empty() && cache_root.is_absolute() &&
            cache_root.filename() == "deep-audit" &&
            cache_root.parent_path().is_absolute(),
        "deep-audit cache uses an absolute application-owned location");
  const auto saves_directory =
      off::platform::application_project_saves_directory();
  check(!saves_directory.empty() && saves_directory.is_absolute() &&
            saves_directory.filename() == "saves" &&
            saves_directory.parent_path().is_absolute() &&
            saves_directory.parent_path() != cache_root,
        "project saves reserve an absolute application-owned directory");
  {
    StartupWindow empty;
  }
  check(SDL_WasInit(SDL_INIT_VIDEO) != 0,
        "empty handoff does not terminate another SDL lifetime");
  SDL_WindowID identity{};
  {
    StartupWindow original{SDL_CreateWindow("Startup ownership test", 64, 64, 0)};
    check(original != nullptr, "create window");
    identity = SDL_GetWindowID(original.get());
    check(SDL_GetWindowSurface(original.get()) != nullptr,
          "create splash software surface");
    check(SDL_UpdateWindowSurface(original.get()), "present software surface");
    off::platform::StartupPreflightResult handoff;
    handoff.window = std::move(original);
    check(!original && SDL_GetWindowID(handoff.window.get()) == identity,
          "preflight handoff preserves window identity");
    StartupWindow runtime_owner = std::move(handoff.window);
    check(!handoff.window && SDL_GetWindowFromID(identity) == runtime_owner.get(),
          "main ownership move preserves the same live window");
    check(SDL_WindowHasSurface(runtime_owner.get()),
          "software surface survives ownership transfer");
    check(SDL_DestroyWindowSurface(runtime_owner.get()),
          "release software surface before GPU claim");
    check(!SDL_WindowHasSurface(runtime_owner.get()) &&
              SDL_GetWindowFromID(identity) == runtime_owner.get(),
          "surface release does not replace or destroy the window");
    check(SDL_WasInit(SDL_INIT_VIDEO) != 0, "video survives surface handoff");
  }
  check(SDL_WasInit(0) == 0, "final owner shuts down SDL after window teardown");
  std::cout << "startup window ownership tests passed\n";
}
