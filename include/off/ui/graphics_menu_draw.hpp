#pragma once

#include "off/ui/graphics_menu.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace off::ui {

struct UiExtent {
  std::uint32_t width{};
  std::uint32_t height{};
  auto operator<=>(const UiExtent &) const = default;
};

struct UiRect {
  float x{};
  float y{};
  float width{};
  float height{};
  auto operator<=>(const UiRect &) const = default;
};

struct UiColor {
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{};
  auto operator<=>(const UiColor &) const = default;
};

enum class UiLayer : std::uint8_t { backdrop, panel, content, focus, modal };
enum class UiControl : std::uint8_t {
  none,
  profile,
  window_mode,
  window_size,
  present_mode,
  render_scale,
  upscaler,
  shadows,
  apply,
  cancel,
  defaults,
  keep,
  revert
};

struct UiRectCommand {
  UiLayer layer{};
  UiRect bounds{};
  UiColor color{};
  auto operator<=>(const UiRectCommand &) const = default;
};

struct UiTextCommand {
  UiLayer layer{};
  UiRect clip{};
  float x{};
  float y{};
  UiColor color{};
  std::string text;
  auto operator<=>(const UiTextCommand &) const = default;
};

struct UiHitTarget {
  UiRect bounds{};
  UiControl control{UiControl::none};
  bool enabled{};
  auto operator<=>(const UiHitTarget &) const = default;
};

enum class UiBuildStatus : std::uint8_t {
  ok,
  invalid_viewport,
  invalid_scale,
  capacity_exceeded
};

inline constexpr std::size_t maximum_ui_rects = 64;
inline constexpr std::size_t maximum_ui_texts = 64;
inline constexpr std::size_t maximum_ui_hit_targets = 16;
inline constexpr std::size_t maximum_ui_text_bytes = 4096;

struct GraphicsMenuDrawList {
  UiExtent target{};
  float ui_scale{1.0F};
  UiBuildStatus status{UiBuildStatus::ok};
  std::vector<UiRectCommand> rectangles;
  std::vector<UiTextCommand> texts;
  std::vector<UiHitTarget> hit_targets;
  auto operator<=>(const GraphicsMenuDrawList &) const = default;
};

struct DiagnosticAsciiAtlas {
  static constexpr std::uint32_t glyph_width = 8;
  static constexpr std::uint32_t glyph_height = 16;
  static constexpr std::uint32_t columns = 16;
  static constexpr std::uint32_t rows = 6;
  UiExtent extent{columns * glyph_width, rows *glyph_height};
  std::vector<std::uint8_t> alpha;
};

[[nodiscard]] DiagnosticAsciiAtlas make_diagnostic_ascii_atlas();
[[nodiscard]] GraphicsMenuDrawList build_graphics_menu_draw_list(
    const GraphicsMenuSession &menu, UiExtent physical_target,
    GraphicsClock::time_point now, float ui_scale = 1.0F);
[[nodiscard]] UiControl hit_test(const GraphicsMenuDrawList &list, float x,
                                 float y) noexcept;
[[nodiscard]] bool
validate_graphics_menu_draw_list(const GraphicsMenuDrawList &list) noexcept;

} // namespace off::ui
