#include "off/platform/sdl_startup.hpp"

#include "off/platform/sdl_locale.hpp"
#include "off/platform/startup_data_error_presentation.hpp"
#include "off/platform/startup_lifecycle.hpp"
#include "off/platform/startup_preparation.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
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

#ifndef OFF_VERSION
#error "OFF_VERSION must be supplied by the CMake release version"
#endif

constexpr std::string_view splash_version = "v" OFF_VERSION;
constexpr std::string_view splash_credit = "Daniel Nylander";

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

[[nodiscard]] std::array<unsigned char, 7> splash_glyph(char character) {
  if (character >= 'a' && character <= 'z')
    character = static_cast<char>(character - 'a' + 'A');
  switch (character) {
  case 'A': return {14, 17, 17, 31, 17, 17, 17};
  case 'D': return {30, 17, 17, 17, 17, 17, 30};
  case 'E': return {31, 16, 16, 30, 16, 16, 31};
  case 'I': return {31, 4, 4, 4, 4, 4, 31};
  case 'L': return {16, 16, 16, 16, 16, 16, 31};
  case 'N': return {17, 25, 21, 19, 17, 17, 17};
  case 'R': return {30, 17, 17, 30, 20, 18, 17};
  case 'V': return {17, 17, 17, 17, 17, 10, 4};
  case 'Y': return {17, 17, 10, 4, 4, 4, 4};
  case '0': return {14, 17, 19, 21, 25, 17, 14};
  case '1': return {4, 12, 4, 4, 4, 4, 14};
  case '2': return {14, 17, 1, 2, 4, 8, 31};
  case '3': return {30, 1, 1, 14, 1, 1, 30};
  case '4': return {2, 6, 10, 18, 31, 2, 2};
  case '5': return {31, 16, 16, 30, 1, 1, 30};
  case '6': return {14, 16, 16, 30, 17, 17, 14};
  case '7': return {31, 1, 2, 4, 8, 8, 8};
  case '8': return {14, 17, 17, 14, 17, 17, 14};
  case '9': return {14, 17, 17, 15, 1, 1, 14};
  case '.': return {0, 0, 0, 0, 0, 12, 12};
  case ' ': return {0, 0, 0, 0, 0, 0, 0};
  default: return {31, 17, 2, 4, 8, 0, 8};
  }
}

void draw_splash_text(SDL_Surface *target, int left, int top,
                      std::string_view text, int pixel_size,
                      Uint32 color) {
  for (const char character : text) {
    const auto glyph = splash_glyph(character);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph[static_cast<std::size_t>(row)] &
             (1U << static_cast<unsigned>(4 - column))) == 0U)
          continue;
        const SDL_Rect pixel{left + column * pixel_size,
                             top + row * pixel_size, pixel_size, pixel_size};
        static_cast<void>(SDL_FillSurfaceRect(target, &pixel, color));
      }
    }
    left += 6 * pixel_size;
  }
}

void draw_splash_overlays(SDL_Surface *target) {
  const int scale = std::max(1, std::min(target->w / 640, target->h / 360));
  const int pixel_size = 2 * scale;
  const int margin = 18 * scale;
  const int text_height = 7 * pixel_size;
  const int credit_width = static_cast<int>(splash_credit.size()) * 6 * pixel_size;
  const int baseline = target->h - margin - text_height;
  const auto shadow = SDL_MapSurfaceRGB(target, 0, 0, 0);
  const auto foreground = SDL_MapSurfaceRGB(target, 238, 238, 232);
  draw_splash_text(target, margin + scale, baseline + scale, splash_version,
                   pixel_size, shadow);
  draw_splash_text(target, margin, baseline, splash_version, pixel_size,
                   foreground);
  draw_splash_text(target, target->w - margin - credit_width + scale,
                   baseline + scale, splash_credit, pixel_size, shadow);
  draw_splash_text(target, target->w - margin - credit_width, baseline,
                   splash_credit, pixel_size, foreground);
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
         (draw_splash_overlays(target), true) &&
         SDL_UpdateWindowSurface(window);
}

void draw_loading_surface(SDL_Window *window) {
  if (SDL_Surface *target = SDL_GetWindowSurface(window); target != nullptr) {
    static_cast<void>(SDL_FillSurfaceRect(
        target, nullptr, SDL_MapSurfaceRGB(target, 10, 13, 18)));
    static_cast<void>(SDL_UpdateWindowSurface(window));
  }
}

[[nodiscard]] StartupPreflightResult
run_sdl_startup_preflight_impl(const std::filesystem::path &data_path,
                             const std::function<void()> &prepare_assets,
                             std::string_view explicit_locale) {
  if (!SDL_Init(SDL_INIT_VIDEO))
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message =
                "SDL initialization failed: " + std::string{SDL_GetError()}};
  SdlSession session;
  // Capture the platform preference once for this launch. The explicit value
  // remains a testing/user override; the startup UI must not infer a locale
  // from the game-data directory or terminal environment.
  const auto platform_locale = preferred_system_locale();

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
    const auto presentation = make_startup_data_error_presentation(
        preparation.verification, ui::l10n::f10_catalog(), explicit_locale,
        platform_locale);
    result.message = presentation.dialog_text();
    // A late verification result may arrive after the loading surface replaced
    // the timed splash. Restore the artwork behind the error dialog.
    static_cast<void>(draw_splash(window.get(), image.get()));
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                  presentation.title.c_str(),
                                  result.message.c_str(), window.get())) {
      // A parented dialog can be unavailable on a compositor even though the
      // process still has a usable SDL video session. Retry without a parent;
      // `result.message` remains returned for the CLI stderr fallback either
      // way.
      static_cast<void>(SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, presentation.title.c_str(), result.message.c_str(),
          nullptr));
    }
  } else if (preparation.outcome == StartupPreparationOutcome::preparation_error) {
    result.outcome = StartupPreflightOutcome::platform_error;
    result.message = preparation.message;
    static_cast<void>(draw_splash(window.get(), image.get()));
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                  "Startup data loading failed",
                                  result.message.c_str(), window.get())) {
      static_cast<void>(SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, "Startup data loading failed",
          result.message.c_str(), nullptr));
    }
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
                          const std::function<void()> &prepare_assets,
                          std::string_view explicit_locale) {
  try {
    return run_sdl_startup_preflight_impl(data_path, prepare_assets,
                                          explicit_locale);
  } catch (...) {
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message = "Native startup encountered an unexpected error"};
  }
}

} // namespace off::platform
