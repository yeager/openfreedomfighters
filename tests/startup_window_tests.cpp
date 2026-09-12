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
  check(overlay.pixel_size == 4, "splash overlay scales for 1280x720");
  check(overlay.version_left == 36 && overlay.baseline == 656,
        "version is placed at the lower-left splash margin");
  check(overlay.credit_left == 884 && overlay.baseline == 656,
        "credit is placed at the lower-right splash margin");
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
