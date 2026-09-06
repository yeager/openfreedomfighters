#include "off/platform/sdl_startup.hpp"

#include "off/platform/startup_lifecycle.hpp"
#include "off/platform/startup_preparation.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <string>

namespace off::platform {

void StartupWindowDeleter::operator()(SDL_Window *window) const noexcept {
  if (window != nullptr) {
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
}

namespace {

constexpr int startup_width = 1280;
constexpr int startup_height = 720;

struct SdlSession {
  SdlSession() = default;
  SdlSession(const SdlSession &) = delete;
  SdlSession &operator=(const SdlSession &) = delete;
  bool owns_lifetime{true};
  ~SdlSession() {
    if (owns_lifetime)
      SDL_Quit();
  }
};

struct WindowDeleter {
  void operator()(SDL_Window *window) const noexcept {
    if (window != nullptr)
      SDL_DestroyWindow(window);
  }
};

struct SurfaceDeleter {
  void operator()(SDL_Surface *surface) const noexcept {
    if (surface != nullptr)
      SDL_DestroySurface(surface);
  }
};

using Window = std::unique_ptr<SDL_Window, WindowDeleter>;
using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

[[nodiscard]] std::filesystem::path splash_path() {
  const char *base = SDL_GetBasePath();
  if (base == nullptr || *base == '\0')
    return {};
  return std::filesystem::path{base} / "assets" /
         "openfreedomfighters-splash.bmp";
}

[[nodiscard]] bool draw_splash(SDL_Window *window, SDL_Surface *image) {
  SDL_Surface *target = SDL_GetWindowSurface(window);
  if (target == nullptr || target->w <= 0 || target->h <= 0)
    return false;
  if (!SDL_FillSurfaceRect(target, nullptr, SDL_MapSurfaceRGB(target, 0, 0, 0)))
    return false;

  const double scale = std::min(static_cast<double>(target->w) / image->w,
                                static_cast<double>(target->h) / image->h);
  const int width = std::max(1, static_cast<int>(image->w * scale));
  const int height = std::max(1, static_cast<int>(image->h * scale));
  const SDL_Rect destination{(target->w - width) / 2, (target->h - height) / 2,
                             width, height};
  return SDL_BlitSurfaceScaled(image, nullptr, target, &destination,
                               SDL_SCALEMODE_LINEAR) &&
         SDL_UpdateWindowSurface(window);
}

void draw_loading_surface(SDL_Window *window) {
  if (SDL_Surface *target = SDL_GetWindowSurface(window); target != nullptr) {
    static_cast<void>(SDL_FillSurfaceRect(
        target, nullptr, SDL_MapSurfaceRGB(target, 10, 13, 18)));
    static_cast<void>(SDL_UpdateWindowSurface(window));
  }
}

[[nodiscard]] std::string
data_error_message(const std::filesystem::path &path,
                   const data::InstallVerification &verification) {
  std::string message =
      "OpenFreedomFighters requires game data from a legally purchased "
      "Freedom Fighters Steam installation.\n\n";
  if (path.empty()) {
    message += "No game-data folder was selected.\n";
  } else {
    message += "The selected game-data folder could not be used.\n";
  }
  message +=
      "Problem: " + verification.message + "\n\nRelaunch with: --data PATH";
  return message;
}

[[nodiscard]] StartupPreflightResult
run_sdl_startup_preflight_impl(const std::filesystem::path &data_path,
                             const std::function<void()> &prepare_assets) {
  if (!SDL_Init(SDL_INIT_VIDEO))
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message =
                "SDL initialization failed: " + std::string{SDL_GetError()}};
  SdlSession session;

  Window window{
      SDL_CreateWindow("OpenFreedomFighters", startup_width, startup_height,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)};
  if (window == nullptr)
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message =
                "SDL window creation failed: " + std::string{SDL_GetError()}};

  const auto image_path = splash_path();
  const auto image_path_text = image_path.u8string();
  Surface image{image_path.empty() ? nullptr
                                   : SDL_LoadBMP(reinterpret_cast<const char *>(
                                         image_path_text.c_str()))};
  if (image == nullptr || !draw_splash(window.get(), image.get()))
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message =
                "Splash presentation failed: " + std::string{SDL_GetError()}};

  StartupLifecycle lifecycle;
  lifecycle.presented(StartupClock::now());
  std::atomic_bool cancelled{false};
  std::future<StartupPreparationResult> verification_future;
  try {
    verification_future = std::async(std::launch::async, [&] {
      return prepare_startup_cpu([&] { return data::verify_install(data_path, [&] { return cancelled.load(); }); },
                                 prepare_assets, cancelled);
    });
  } catch (...) {
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message = "Could not start game-data verification and preparation"};
  }
  bool loading_surface_presented = false;
  while (lifecycle.phase() != StartupPhase::ready &&
         lifecycle.phase() != StartupPhase::cancelled) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT ||
          event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
          (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
           event.key.key == SDLK_ESCAPE)) {
        lifecycle.cancel();
        cancelled.store(true);
      } else if (event.type == SDL_EVENT_WINDOW_EXPOSED ||
                 event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        if (lifecycle.phase() == StartupPhase::splash)
          static_cast<void>(draw_splash(window.get(), image.get()));
        else
          draw_loading_surface(window.get());
      }
    }
    const bool ready = verification_future.wait_for(std::chrono::seconds{0}) ==
                       std::future_status::ready;
    const auto phase = lifecycle.tick(StartupClock::now(), ready);
    if (phase == StartupPhase::loading && !loading_surface_presented) {
      draw_loading_surface(window.get());
      loading_surface_presented = true;
    }
    if (phase != StartupPhase::ready && phase != StartupPhase::cancelled)
      SDL_Delay(4);
  }

  if (lifecycle.phase() == StartupPhase::cancelled)
    static_cast<void>(SDL_HideWindow(window.get()));
  // std::future owns a real worker. Await it before tearing down process state;
  // a cancelled launch hides immediately but never abandons an active parser.
  StartupPreparationResult preparation;
  try {
    preparation = verification_future.get();
  } catch (...) {
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message = "Startup work did not return a result"};
  }
  StartupPreflightResult result;
  result.verification = preparation.verification;
  if (lifecycle.phase() == StartupPhase::cancelled ||
      preparation.outcome == StartupPreparationOutcome::cancelled) {
    result.outcome = StartupPreflightOutcome::quit_requested;
    result.message = "Startup cancelled";
  } else if (preparation.outcome == StartupPreparationOutcome::verification_error) {
    result.outcome = StartupPreflightOutcome::data_error;
    result.message = data_error_message(data_path, preparation.verification);
    // A late verification result may arrive after the loading surface replaced
    // the timed splash. Restore the artwork behind the error dialog.
    static_cast<void>(draw_splash(window.get(), image.get()));
    static_cast<void>(
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data required",
                                 result.message.c_str(), window.get()));
  } else if (preparation.outcome == StartupPreparationOutcome::preparation_error) {
    result.outcome = StartupPreflightOutcome::platform_error;
    result.message = preparation.message;
    static_cast<void>(draw_splash(window.get(), image.get()));
    static_cast<void>(SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR, "Startup data loading failed",
        result.message.c_str(), window.get()));
  } else {
    result.outcome = StartupPreflightOutcome::ready;
    result.message = preparation.message;
    result.window.reset(window.release());
    session.owns_lifetime = false;
  }

  return result;
}

} // namespace

StartupPreflightResult
run_sdl_startup_preflight(const std::filesystem::path &data_path,
                          const std::function<void()> &prepare_assets) {
  try {
    return run_sdl_startup_preflight_impl(data_path, prepare_assets);
  } catch (...) {
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message = "Native startup encountered an unexpected error"};
  }
}

} // namespace off::platform
