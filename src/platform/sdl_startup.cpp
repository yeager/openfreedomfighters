#include "off/platform/sdl_startup.hpp"

#include "off/platform/sdl_locale.hpp"
#include "off/platform/startup_data_error_presentation.hpp"
#include "off/platform/startup_lifecycle.hpp"
#include "off/platform/startup_preparation.hpp"
#include "off/ui/project_localization.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace off::platform {

void StartupWindowDeleter::operator()(SDL_Window *window) const noexcept {
  if (window != nullptr) {
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
}

#ifndef OFF_VERSION
#error "OFF_VERSION must be supplied by the CMake release version"
#endif

constexpr std::string_view splash_version = "v" OFF_VERSION;
constexpr std::string_view splash_credit = "Daniel Nylander";

StartupSplashOverlayLayout startup_splash_overlay_layout(int width,
                                                         int height) noexcept {
  const int scale = std::max(1, std::min(width / 640, height / 360));
  const int pixel_size = 2 * scale;
  const int margin = 18 * scale;
  const int text_height = 7 * pixel_size;
  const int credit_width =
      static_cast<int>(splash_credit.size()) * 6 * pixel_size;
  return {.version = splash_version,
          .credit = splash_credit,
          .pixel_size = pixel_size,
          .version_left = margin,
          .credit_left = width - margin - credit_width,
          .baseline = height - margin - text_height};
}

StartupDataErrorBackdropLayout
startup_data_error_backdrop_layout(int width, int height) noexcept {
  // The window surface normally has positive dimensions. Keep this pure
  // layout total for tests and for a transient zero-sized resize callback.
  width = std::max(1, width);
  height = std::max(1, height);
  const int margin = std::max(1, std::min(width, height) / 24);
  const int available_width = std::max(1, width - 2 * margin);
  const int available_height = std::max(1, height - 2 * margin);
  const int panel_width = std::min(available_width, std::max(1, width * 3 / 4));
  const int panel_height =
      std::min(available_height, std::max(1, height / 3));
  return {.left = (width - panel_width) / 2,
          .top = (height - panel_height) / 2,
          .width = panel_width,
          .height = panel_height};
}

std::filesystem::path application_deep_audit_cache_root() noexcept {
  // This path is also used by headless integrity checks. SDL's preference-path
  // implementation keeps process-global environment/TLS state on some
  // platforms, which makes that otherwise short-lived check look like a leak
  // to LeakSanitizer. Use the same native per-user cache policy directly.
#if defined(_WIN32)
  if (const auto *local_app_data = std::getenv("LOCALAPPDATA");
      local_app_data && *local_app_data) {
    const std::filesystem::path base{local_app_data};
    if (base.is_absolute())
      return base / "OpenFreedomFighters" / "deep-audit";
  }
#elif defined(__APPLE__)
  if (const auto *home = std::getenv("HOME"); home && *home) {
    const std::filesystem::path base{home};
    if (base.is_absolute())
      return base / "Library" / "Caches" / "OpenFreedomFighters" / "deep-audit";
  }
#else
  if (const auto *xdg_cache = std::getenv("XDG_CACHE_HOME");
      xdg_cache && *xdg_cache) {
    const std::filesystem::path base{xdg_cache};
    if (base.is_absolute())
      return base / "openfreedomfighters" / "deep-audit";
  }
  if (const auto *home = std::getenv("HOME"); home && *home) {
    const std::filesystem::path base{home};
    if (base.is_absolute())
      return base / ".cache" / "openfreedomfighters" / "deep-audit";
  }
#endif
  return {};
}

std::filesystem::path application_graphics_settings_path() noexcept {
  char *raw_path =
      SDL_GetPrefPath("OpenFreedomFighters", "OpenFreedomFighters");
  if (raw_path == nullptr)
    return {};
  std::unique_ptr<char, decltype(&SDL_free)> path{raw_path, SDL_free};
  if (*path == '\0')
    return {};
  const std::filesystem::path directory{path.get()};
  if (!directory.is_absolute())
    return {};
  return directory / "graphics.settings";
}

std::filesystem::path application_translation_packs_directory() noexcept {
  char *raw_path =
      SDL_GetPrefPath("OpenFreedomFighters", "OpenFreedomFighters");
  if (raw_path == nullptr)
    return {};
  std::unique_ptr<char, decltype(&SDL_free)> path{raw_path, SDL_free};
  if (*path == '\0')
    return {};
  const std::filesystem::path directory{path.get()};
  if (!directory.is_absolute())
    return {};
  return directory / "translation-packs";
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

[[nodiscard]] std::filesystem::path splash_font_path() {
  const char *base = SDL_GetBasePath();
  if (base == nullptr || *base == '\0')
    return {};
  return std::filesystem::path{base} / "assets" / "Rajdhani-SemiBold.ttf";
}

struct FontDeleter {
  void operator()(TTF_Font *font) const noexcept {
    if (font != nullptr)
      TTF_CloseFont(font);
  }
};

[[nodiscard]] bool draw_splash_text(SDL_Surface *target, TTF_Font *font,
                                    std::string_view text, int left,
                                    int baseline, SDL_Color color) {
  Surface rendered{
      TTF_RenderText_Blended(font, text.data(), text.size(), color)};
  if (rendered == nullptr)
    return false;
  const SDL_Rect destination{left, baseline - rendered->h, rendered->w,
                             rendered->h};
  return SDL_BlitSurface(rendered.get(), nullptr, target, &destination);
}

void draw_splash_overlays(SDL_Surface *target) {
  const auto layout = startup_splash_overlay_layout(target->w, target->h);
  if (!TTF_Init())
    return;
  const auto font_path_text = splash_font_path().u8string();
  std::unique_ptr<TTF_Font, FontDeleter> font{
      TTF_OpenFont(reinterpret_cast<const char *>(font_path_text.c_str()),
                   static_cast<float>(std::max(18, target->h / 27)))};
  if (font == nullptr) {
    TTF_Quit();
    return;
  }
  const SDL_Color shadow{0, 0, 0, 220};
  const SDL_Color foreground{238, 238, 232, 255};
  constexpr int shadow_offset = 2;
  const int margin = std::max(18, target->h / 40);
  int credit_width = 0;
  if (!TTF_GetStringSize(font.get(), layout.credit.data(), layout.credit.size(),
                         &credit_width, nullptr)) {
    font.reset();
    TTF_Quit();
    return;
  }
  const int credit_left = target->w - margin - credit_width;
  static_cast<void>(draw_splash_text(
      target, font.get(), layout.version, margin + shadow_offset,
      target->h - margin + shadow_offset, shadow));
  static_cast<void>(draw_splash_text(target, font.get(), layout.version, margin,
                                     target->h - margin, foreground));
  static_cast<void>(draw_splash_text(
      target, font.get(), layout.credit, credit_left + shadow_offset,
      target->h - margin + shadow_offset, shadow));
  static_cast<void>(draw_splash_text(target, font.get(), layout.credit,
                                     credit_left, target->h - margin,
                                     foreground));
  font.reset();
  TTF_Quit();
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

void draw_loading_surface(SDL_Window *window, std::string_view status) {
  SDL_Surface *target = SDL_GetWindowSurface(window);
  if (target == nullptr)
    return;
  static_cast<void>(SDL_FillSurfaceRect(target, nullptr,
                                        SDL_MapSurfaceRGB(target, 10, 13, 18)));
  if (TTF_Init()) {
    const auto font_path_text = splash_font_path().u8string();
    TTF_Font *font =
        TTF_OpenFont(reinterpret_cast<const char *>(font_path_text.c_str()),
                     static_cast<float>(std::max(20, target->h / 25)));
    if (font != nullptr) {
      int text_width{};
      if (TTF_GetStringSize(font, status.data(), status.size(), &text_width,
                            nullptr)) {
        static_cast<void>(
            draw_splash_text(target, font, status, (target->w - text_width) / 2,
                             target->h / 2, SDL_Color{238, 238, 232, 255}));
      }
      // SDL_ttf owns the backing FreeType library. Close the font while that
      // library is still alive, before TTF_Quit().
      TTF_CloseFont(font);
    }
    TTF_Quit();
  }
  static_cast<void>(SDL_UpdateWindowSurface(window));
}

void draw_data_error_backdrop(SDL_Window *window) {
  SDL_Surface *target = SDL_GetWindowSurface(window);
  if (target == nullptr || target->w <= 0 || target->h <= 0)
    return;
  const auto layout = startup_data_error_backdrop_layout(target->w, target->h);
  const SDL_Rect panel{layout.left, layout.top, layout.width, layout.height};
  const SDL_Color panel_colour{layout.panel.red, layout.panel.green,
                               layout.panel.blue, 255};
  const SDL_Color border_colour{layout.border.red, layout.border.green,
                                layout.border.blue, 255};
  static_cast<void>(SDL_FillSurfaceRect(
      target, &panel, SDL_MapSurfaceRGB(target, panel_colour.r,
                                        panel_colour.g, panel_colour.b)));
  const Uint32 border_pixel = SDL_MapSurfaceRGB(
      target, border_colour.r, border_colour.g, border_colour.b);
  const int border_width = std::min(2, std::min(panel.w, panel.h));
  const SDL_Rect top{panel.x, panel.y, panel.w, border_width};
  const SDL_Rect bottom{panel.x, panel.y + panel.h - border_width, panel.w,
                        border_width};
  const SDL_Rect left{panel.x, panel.y, border_width, panel.h};
  const SDL_Rect right{panel.x + panel.w - border_width, panel.y,
                       border_width, panel.h};
  static_cast<void>(SDL_FillSurfaceRect(target, &top, border_pixel));
  static_cast<void>(SDL_FillSurfaceRect(target, &bottom, border_pixel));
  static_cast<void>(SDL_FillSurfaceRect(target, &left, border_pixel));
  static_cast<void>(SDL_FillSurfaceRect(target, &right, border_pixel));
  static_cast<void>(SDL_UpdateWindowSurface(window));
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
  const auto platform_locale_storage = preferred_system_locales();
  std::vector<std::string_view> platform_locales;
  platform_locales.reserve(platform_locale_storage.size());
  for (const auto &locale : platform_locale_storage)
    platform_locales.push_back(locale);

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
  std::atomic<StartupPreparationStage> preparation_stage{
      StartupPreparationStage::verifying_game_data};
  const auto loading_status = [&] {
    const auto id =
        preparation_stage.load() == StartupPreparationStage::preparing_assets
            ? ui::l10n::MessageId::preparing_startup
            : ui::l10n::MessageId::verifying_game_data;
    return ui::l10n::f10_catalog()
        .resolve(id, explicit_locale, platform_locales)
        .value_or("Preparing startup...");
  };
  std::future<StartupPreparationResult> verification_future;
  try {
    verification_future = std::async(std::launch::async, [&] {
      return prepare_startup_cpu(
          [&]() {
            return data::verify_install(
                data_path, [&] { return cancelled.load(); },
                {.deep_audit_cache_root = application_deep_audit_cache_root()});
          },
          prepare_assets, cancelled,
          [&](StartupPreparationStage stage) {
            preparation_stage.store(stage);
          },
          [&]() {
            return data::verify_install(
                data_path, [&] { return cancelled.load(); },
                {.deep_audit_cache_root = application_deep_audit_cache_root()});
          });
    });
  } catch (...) {
    return {.outcome = StartupPreflightOutcome::platform_error,
            .message =
                "Could not start game-data verification and preparation"};
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
          draw_loading_surface(window.get(), loading_status());
      }
    }
    const bool ready = verification_future.wait_for(std::chrono::seconds{0}) ==
                       std::future_status::ready;
    const auto phase = lifecycle.tick(StartupClock::now(), ready);
    if (phase == StartupPhase::loading && !loading_surface_presented) {
      draw_loading_surface(window.get(), loading_status());
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
  } else if (preparation.outcome ==
             StartupPreparationOutcome::verification_error) {
    result.outcome = StartupPreflightOutcome::data_error;
    const auto presentation = make_startup_data_error_presentation(
        preparation.verification, ui::l10n::f10_catalog(), explicit_locale,
        platform_locales);
    result.message = presentation.dialog_text();
    // A late verification result may arrive after the loading surface replaced
    // the timed splash. Restore the artwork behind the error dialog.
    static_cast<void>(draw_splash(window.get(), image.get()));
    draw_data_error_backdrop(window.get());
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                  presentation.title.c_str(),
                                  result.message.c_str(), window.get())) {
      // A parented dialog can be unavailable on a compositor even though the
      // process still has a usable SDL video session. Retry without a parent;
      // `result.message` remains returned for the CLI stderr fallback either
      // way.
      static_cast<void>(SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, presentation.title.c_str(),
          result.message.c_str(), nullptr));
    }
  } else if (preparation.outcome ==
             StartupPreparationOutcome::preparation_error) {
    // Verification already passed, but parsing the checked source set did not.
    // Present this through the same localized, path-free data-error surface.
    // `preparation.message` can originate from filesystem/parser exceptions,
    // so it is not exposed through either the popup or this public result.
    result.outcome = StartupPreflightOutcome::data_error;
    auto display_verification = result.verification;
    display_verification.error = data::InstallError::io_error;
    const auto presentation = make_startup_data_error_presentation(
        display_verification, ui::l10n::f10_catalog(), explicit_locale,
        platform_locales);
    result.message = presentation.dialog_text();
    static_cast<void>(draw_splash(window.get(), image.get()));
    draw_data_error_backdrop(window.get());
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                  presentation.title.c_str(),
                                  result.message.c_str(), window.get())) {
      static_cast<void>(SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, presentation.title.c_str(),
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
