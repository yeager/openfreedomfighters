#pragma once

#include "off/mode.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace off::settings {

enum class WindowMode : std::uint8_t { windowed, borderless_desktop };
enum class PresentMode : std::uint8_t { vsync, mailbox, immediate };
// Vendor backends are retained as user intent.  A backend becomes effective
// only when the renderer has loaded and validated its actual implementation.
enum class Upscaler : std::uint8_t { native, temporal, dlss, fsr, xess };
enum class ShadowQuality : std::uint8_t { reference, high, ultra };

struct WindowSize {
  std::uint32_t width{1280};
  std::uint32_t height{720};
  friend bool operator==(const WindowSize &, const WindowSize &) = default;
};

struct RequestedGraphicsSettings {
  Mode profile{Mode::original};
  WindowMode window_mode{WindowMode::windowed};
  WindowSize windowed_size{};
  PresentMode present_mode{PresentMode::vsync};
  bool modern_plus{false};
  std::uint16_t render_scale_percent{100};
  Upscaler upscaler{Upscaler::native};
  ShadowQuality shadow_quality{ShadowQuality::reference};
  friend bool operator==(const RequestedGraphicsSettings &,
                         const RequestedGraphicsSettings &) = default;
};

struct GraphicsCapabilities {
  bool original_profile{true};
  bool modern_profile{true};
  bool borderless_desktop{true};
  bool mailbox_present{false};
  bool immediate_present{false};
  bool modern_plus{true};
  bool temporal_upscaler{true};
  bool dlss_upscaler{false};
  bool fsr_upscaler{false};
  bool xess_upscaler{false};
  WindowSize minimum_windowed_size{640, 360};
  WindowSize maximum_windowed_size{16384, 16384};
};

enum class GraphicsField : std::uint8_t {
  profile,
  window_mode,
  windowed_size,
  present_mode,
  modern_plus,
  upscaler
};
enum class FallbackReason : std::uint8_t {
  profile_unavailable,
  borderless_unavailable,
  mailbox_unavailable,
  immediate_unavailable,
  modern_plus_unavailable,
  modern_plus_upscaler_required,
  temporal_upscaler_unavailable,
  dlss_upscaler_unavailable,
  fsr_upscaler_unavailable,
  xess_upscaler_unavailable
};

struct GraphicsFallback {
  GraphicsField field{GraphicsField::profile};
  FallbackReason reason{FallbackReason::profile_unavailable};
  friend bool operator==(const GraphicsFallback &,
                         const GraphicsFallback &) = default;
};

struct EffectiveGraphicsSettings {
  Mode profile{Mode::original};
  WindowMode window_mode{WindowMode::windowed};
  WindowSize windowed_size{};
  PresentMode present_mode{PresentMode::vsync};
  bool modern_plus{false};
  std::uint16_t render_scale_percent{100};
  Upscaler upscaler{Upscaler::native};
  ShadowQuality shadow_quality{ShadowQuality::reference};
  std::vector<GraphicsFallback> fallbacks;
  friend bool operator==(const EffectiveGraphicsSettings &,
                         const EffectiveGraphicsSettings &) = default;
};

enum class GraphicsValidationError : std::uint8_t {
  invalid_enum,
  zero_window_dimension,
  window_size_below_minimum,
  window_size_above_maximum,
  no_supported_profile,
  render_scale_out_of_range
};

struct GraphicsResolution {
  std::optional<EffectiveGraphicsSettings> effective;
  std::optional<GraphicsValidationError> error;
};

// Startup preferences can follow a player between displays.  Unlike an
// interactive request, a previously valid windowed extent may be outside the
// current native backend's bounds (for example after moving from a desktop to
// Steam Deck).  Keep that narrow recovery explicit: only extent-bound errors
// may be clamped, and the caller retains the adjusted request for its live
// session without rewriting the stored preference.
struct InitialGraphicsResolution {
  RequestedGraphicsSettings requested;
  GraphicsResolution resolution;
  bool recovered_windowed_size{false};
};

enum class InitialGraphicsSetup : std::uint8_t {
  ready,
  invalid_resolution,
  apply_failed,
};

// Applying a display setting may fail after the native backend has changed part
// of its state.  Keep that recovery result distinct from an ordinary rejected
// apply so callers never continue while claiming a known display state that
// could not be restored.
enum class GraphicsApplyTransaction : std::uint8_t {
  applied,
  restored_previous,
  restore_failed,
};

[[nodiscard]] GraphicsResolution
resolve_graphics_settings(const RequestedGraphicsSettings &requested,
                          const GraphicsCapabilities &capabilities);

[[nodiscard]] InitialGraphicsResolution
resolve_initial_graphics_settings(const RequestedGraphicsSettings &requested,
                                  const GraphicsCapabilities &capabilities);

// The native backend owns the actual display calls.  This small boundary makes
// the boot contract explicit: apply the already-resolved configuration once,
// before any frame is acquired, and surface a backend failure to the caller.
[[nodiscard]] InitialGraphicsSetup initialize_graphics_settings(
    const GraphicsResolution &resolution,
    const std::function<bool(const EffectiveGraphicsSettings &)> &apply);

[[nodiscard]] GraphicsApplyTransaction apply_graphics_transaction(
    const EffectiveGraphicsSettings &before,
    const EffectiveGraphicsSettings &after,
    const std::function<bool(const EffectiveGraphicsSettings &)> &apply);

[[nodiscard]] bool
requires_display_confirmation(const EffectiveGraphicsSettings &before,
                              const EffectiveGraphicsSettings &after) noexcept;

} // namespace off::settings
