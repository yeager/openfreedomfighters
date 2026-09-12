#include "off/platform/sdl_startup.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << ": " << SDL_GetError() << '\n';
    std::exit(1);
  }
}

} // namespace

int main() {
  // This test deliberately enters the same SDL startup function as a normal
  // launch.  The dummy driver proves the missing-data path does not require a
  // compositor or a real display, while still exercising splash setup and its
  // orderly teardown.
  const char *video_driver = SDL_getenv("SDL_VIDEODRIVER");
  check(video_driver != nullptr && std::string_view{video_driver} == "dummy",
        "CTest selects SDL dummy video driver");

  const auto missing_root =
      std::filesystem::path{OFF_TEST_WORK_DIR} / "absent-game-data";
  check(!std::filesystem::exists(missing_root),
        "fixture game-data root must be absent");

  bool prepared_assets = false;
  auto result = off::platform::run_sdl_startup_preflight(
      missing_root, [&] { prepared_assets = true; }, "en");

  check(result.outcome == off::platform::StartupPreflightOutcome::data_error,
        "missing root reports a data error");
  check(result.verification.error == off::data::InstallError::missing_root,
        "missing root preserves verifier error");
  check(result.verification.root == missing_root,
        "missing root preserves verifier diagnostic path");
  check(result.verification.message == "game-data directory does not exist",
        "missing root preserves verifier diagnostic message");
  check(result.message.find("game-data directory does not exist") !=
            std::string::npos,
        "startup dialog result includes technical diagnostic");
  check(!prepared_assets,
        "asset preparation is not called after data verification fails");
  check(!result.window, "failed preflight hands no window to the runtime");
  check(SDL_WasInit(0) == 0,
        "failed preflight destroys its temporary window and SDL session");

  std::cout << "native SDL startup missing-data smoke test passed\n";
  return 0;
}
