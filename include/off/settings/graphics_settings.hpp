#pragma once

#include "off/mode.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace off::settings {

enum class WindowMode : std::uint8_t { windowed, borderless_desktop };
enum class PresentMode : std::uint8_t { vsync, mailbox, immediate };

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
  friend bool operator==(const RequestedGraphicsSettings &,
                         const RequestedGraphicsSettings &) = default;
};

struct GraphicsCapabilities {
  bool original_profile{true};
  bool modern_profile{true};
  bool borderless_desktop{true};
  bool mailbox_present{false};
  bool immediate_present{false};
  WindowSize minimum_windowed_size{640, 360};
  WindowSize maximum_windowed_size{16384, 16384};
};

enum class GraphicsField : std::uint8_t {
  profile,
  window_mode,
  windowed_size,
  present_mode
};
enum class FallbackReason : std::uint8_t {
  profile_unavailable,
  borderless_unavailable,
  mailbox_unavailable,
  immediate_unavailable
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
  std::vector<GraphicsFallback> fallbacks;
  friend bool operator==(const EffectiveGraphicsSettings &,
                         const EffectiveGraphicsSettings &) = default;
};

enum class GraphicsValidationError : std::uint8_t {
  invalid_enum,
  zero_window_dimension,
  window_size_below_minimum,
  window_size_above_maximum,
  no_supported_profile
};

struct GraphicsResolution {
  std::optional<EffectiveGraphicsSettings> effective;
  std::optional<GraphicsValidationError> error;
};

[[nodiscard]] GraphicsResolution
resolve_graphics_settings(const RequestedGraphicsSettings &requested,
                          const GraphicsCapabilities &capabilities);

[[nodiscard]] bool
requires_display_confirmation(const EffectiveGraphicsSettings &before,
                              const EffectiveGraphicsSettings &after) noexcept;

} // namespace off::settings
