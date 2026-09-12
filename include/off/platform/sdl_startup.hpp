#pragma once

#include "off/data/install.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

struct SDL_Window;

namespace off::platform {

struct StartupWindowDeleter {
  // The successful startup handoff exclusively owns window and SDL lifetime.
  // Not a general-purpose deleter for windows from another SDL session.
  void operator()(SDL_Window *window) const noexcept;
};
using StartupWindow = std::unique_ptr<SDL_Window, StartupWindowDeleter>;

enum class StartupPreflightOutcome {
  ready,
  data_error,
  quit_requested,
  platform_error,
};

struct StartupPreflightResult {
  StartupPreflightOutcome outcome{StartupPreflightOutcome::platform_error};
  data::InstallVerification verification;
  std::string message;
  // Non-null only on success. Move-only owner; keep alive while runtime
  // borrows.
  StartupWindow window{};
};

// Immutable layout for the project-owned startup artwork metadata. The text is
// supplied by the build's release version, rather than by a runtime setting.
// Coordinates name the foreground glyph origin; the shadow is drawn one scale
// unit down and right of the same origin.
struct StartupSplashOverlayLayout {
  std::string_view version;
  std::string_view credit;
  int pixel_size{};
  int version_left{};
  int credit_left{};
  int baseline{};
};

// Pure placement contract used by the SDL splash renderer. Width and height
// must be positive window-surface dimensions.
[[nodiscard]] StartupSplashOverlayLayout
startup_splash_overlay_layout(int width, int height) noexcept;

// Project-owned colours and bounds for the startup data-error backing panel.
// SDL's native message box is intentionally still used for accessibility and
// platform conventions, but this panel keeps the underlying startup surface
// legible while that modal is open. Values contain no retail artwork or text.
struct StartupRgb {
  unsigned char red{};
  unsigned char green{};
  unsigned char blue{};
};

struct StartupDataErrorBackdropLayout {
  int left{};
  int top{};
  int width{};
  int height{};
  StartupRgb panel{20, 27, 36};
  StartupRgb border{205, 44, 38};
};

// A centred, in-bounds region behind the native data-error modal. Width and
// height must be positive window-surface dimensions.
[[nodiscard]] StartupDataErrorBackdropLayout
startup_data_error_backdrop_layout(int width, int height) noexcept;

// A platform-selected, application-owned cache location. Failure returns an
// empty path and callers continue without a derived cache.
[[nodiscard]] std::filesystem::path
application_deep_audit_cache_root() noexcept;

// Returns the SDL-owned per-user preferences file for requested graphics
// settings.  There is deliberately no environment or home-directory fallback:
// an unavailable SDL preference location disables persistence for this launch.
[[nodiscard]] std::filesystem::path
application_graphics_settings_path() noexcept;

// Returns the SDL-owned per-user directory used for optional private
// translation packs. An unavailable preferences location disables pack
// enrollment for the launch; no environment or home-directory fallback is
// used.
[[nodiscard]] std::filesystem::path
application_translation_packs_directory() noexcept;

// Opens the project-owned splash before touching retail data. This entry point
// is intentionally not used by --verify-only, --help, or --version.
// prepare_assets runs on the verification worker after successful verification.
// It must perform CPU-only work; captures remain alive until this call returns.
// The worker is always joined before return, including cancellation and errors.
[[nodiscard]] StartupPreflightResult
run_sdl_startup_preflight(const std::filesystem::path &data_path,
                          const std::function<void()> &prepare_assets,
                          std::string_view explicit_locale = {});

} // namespace off::platform
