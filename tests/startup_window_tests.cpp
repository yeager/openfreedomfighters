#include "off/platform/sdl_startup.hpp"

#include <SDL3/SDL.h>

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
