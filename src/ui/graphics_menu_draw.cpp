#include "off/ui/graphics_menu_draw.hpp"

#include <algorithm>
#include <cmath>

namespace off::ui {
namespace {

constexpr UiColor dim{0, 0, 0, 170};
constexpr UiColor panel{20, 25, 35, 245};
constexpr UiColor row{38, 45, 58, 255};
constexpr UiColor focus{232, 176, 55, 255};
constexpr UiColor white{240, 243, 248, 255};
constexpr UiColor muted{166, 174, 187, 255};

bool contains(const UiRect &rect, float x, float y) {
  return x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
         y < rect.y + rect.height;
}

bool inside(const UiRect &rect, UiExtent target) {
  return std::isfinite(rect.x) && std::isfinite(rect.y) &&
         std::isfinite(rect.width) && std::isfinite(rect.height) &&
         rect.width > 0.0F && rect.height > 0.0F && rect.x >= 0.0F &&
         rect.y >= 0.0F && rect.x + rect.width <= static_cast<float>(target.width) &&
         rect.y + rect.height <= static_cast<float>(target.height);
}

std::string profile_name(Mode mode) {
  return mode == Mode::modern ? "Modern" : "Original";
}

std::string window_name(settings::WindowMode mode) {
  return mode == settings::WindowMode::borderless_desktop
             ? "Borderless desktop"
             : "Windowed";
}

std::string present_name(settings::PresentMode mode) {
  switch (mode) {
  case settings::PresentMode::vsync:
    return "VSync";
  case settings::PresentMode::mailbox:
    return "Mailbox";
  case settings::PresentMode::immediate:
    return "Immediate";
  }
  return "Invalid";
}

} // namespace

DiagnosticAsciiAtlas make_diagnostic_ascii_atlas() {
  DiagnosticAsciiAtlas atlas;
  atlas.alpha.assign(static_cast<std::size_t>(atlas.extent.width) *
                         atlas.extent.height,
                     0);
  // A deterministic placeholder cell makes unsupported/missing glyphs visible.
  // Stage 3 replaces these cells with generated Spleen 8x16 bitmaps.
  for (std::uint32_t y = 2; y < 14; ++y) {
    for (std::uint32_t x = 1; x < 7; ++x) {
      if (x == 1 || x == 6 || y == 2 || y == 13) {
        atlas.alpha[static_cast<std::size_t>(y) * atlas.extent.width + x] = 255;
      }
    }
  }
  return atlas;
}

GraphicsMenuDrawList build_graphics_menu_draw_list(
    const GraphicsMenuSession &menu, UiExtent target,
    GraphicsClock::time_point now, float scale) {
  GraphicsMenuDrawList out;
  out.target = target;
  if (target.width == 0 || target.height == 0) {
    out.status = UiBuildStatus::invalid_viewport;
    return out;
  }
  if (!std::isfinite(scale) || scale < 0.5F || scale > 4.0F) {
    out.status = UiBuildStatus::invalid_scale;
    return out;
  }
  if (menu.phase() == GraphicsMenuPhase::closed) {
    return out;
  }

  const float width = static_cast<float>(target.width);
  const float height = static_cast<float>(target.height);
  const float margin = std::min(16.0F * scale, std::min(width, height) * 0.1F);
  const float panel_width = std::max(1.0F, std::min(720.0F * scale, width - 2 * margin));
  const float panel_height = std::max(1.0F, std::min(500.0F * scale, height - 2 * margin));
  const UiRect panel_rect{(width - panel_width) * 0.5F,
                          (height - panel_height) * 0.5F, panel_width,
                          panel_height};
  out.rectangles.push_back({UiLayer::backdrop, {0, 0, width, height}, dim});
  out.rectangles.push_back({UiLayer::panel, panel_rect, panel});

  auto add_text = [&](UiLayer layer, float x, float y, std::string text,
                      UiColor color = white) {
    std::size_t bytes = text.size();
    for (const auto &existing : out.texts) {
      bytes += existing.text.size();
    }
    if (out.texts.size() >= maximum_ui_texts || bytes > maximum_ui_text_bytes) {
      out.status = UiBuildStatus::capacity_exceeded;
      return false;
    }
    out.texts.push_back({layer, panel_rect, x, y, color, std::move(text)});
    return true;
  };
  auto finish = [&]() {
    if (!validate_graphics_menu_draw_list(out)) {
      out = {};
      out.target = target;
      out.status = UiBuildStatus::capacity_exceeded;
    }
    return out;
  };
  const float pad = std::min(28.0F * scale, panel_width * 0.08F);
  if (!add_text(UiLayer::content, panel_rect.x + pad, panel_rect.y + pad,
                "GRAPHICS SETTINGS")) {
    out.rectangles.clear(); out.texts.clear(); return out;
  }

  if (menu.phase() == GraphicsMenuPhase::confirming) {
    auto seconds = 0LL;
    if (const auto deadline = menu.confirmation_deadline()) {
      const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
          *deadline > now ? *deadline - now : GraphicsClock::duration::zero()).count();
      seconds = (milliseconds + 999) / 1000;
    }
    add_text(UiLayer::modal, panel_rect.x + pad, panel_rect.y + 100 * scale,
             "Keep these display settings?");
    add_text(UiLayer::modal, panel_rect.x + pad, panel_rect.y + 130 * scale,
             "Reverting in " + std::to_string(seconds) + " seconds");
    const UiRect keep{panel_rect.x + pad, panel_rect.y + panel_height - 70 * scale,
                      150 * scale, 42 * scale};
    const UiRect revert{keep.x + 170 * scale, keep.y, 150 * scale, keep.height};
    out.hit_targets.push_back({keep, UiControl::keep, seconds > 0});
    out.hit_targets.push_back({revert, UiControl::revert, true});
    add_text(UiLayer::modal, keep.x + 12 * scale, keep.y + 12 * scale, "Keep");
    add_text(UiLayer::modal, revert.x + 12 * scale, revert.y + 12 * scale, "Revert");
    return finish();
  }

  if (menu.phase() == GraphicsMenuPhase::applying ||
      menu.phase() == GraphicsMenuPhase::reverting) {
    add_text(UiLayer::modal, panel_rect.x + pad, panel_rect.y + 110 * scale,
             menu.phase() == GraphicsMenuPhase::applying
                 ? "Applying settings..."
                 : "Restoring settings...");
    return finish();
  }

  const auto &draft = menu.draft();
  const std::array labels{"Profile", "Window mode", "Window size", "Present mode"};
  const std::array values{profile_name(draft.profile), window_name(draft.window_mode),
                          std::to_string(draft.windowed_size.width) + " x " +
                              std::to_string(draft.windowed_size.height),
                          present_name(draft.present_mode)};
  const std::array controls{UiControl::profile, UiControl::window_mode,
                            UiControl::window_size, UiControl::present_mode};
  const float row_height = std::max(34.0F, std::min(52.0F * scale,
      (panel_height - 170.0F * scale) / 6.0F));
  float y = panel_rect.y + 70 * scale;
  for (std::size_t i = 0; i < labels.size(); ++i) {
    const UiRect bounds{panel_rect.x + pad, y, panel_width - 2 * pad, row_height};
    out.rectangles.push_back({UiLayer::content, bounds, row});
    out.hit_targets.push_back({bounds, controls[i], true});
    add_text(UiLayer::content, bounds.x + 12 * scale, bounds.y + 12 * scale,
             labels[i], muted);
    add_text(UiLayer::content, bounds.x + bounds.width * 0.52F,
             bounds.y + 12 * scale, values[i]);
    if (static_cast<std::size_t>(menu.selected_row()) == i) {
      out.rectangles.push_back({UiLayer::focus, bounds, focus});
    }
    y += row_height + 8 * scale;
  }
  const UiRect apply{panel_rect.x + pad, panel_rect.y + panel_height - 62 * scale,
                     150 * scale, 40 * scale};
  const UiRect cancel{apply.x + 170 * scale, apply.y, 150 * scale, apply.height};
  out.hit_targets.push_back({apply, UiControl::apply, true});
  out.hit_targets.push_back({cancel, UiControl::cancel, true});
  add_text(UiLayer::content, apply.x + 12 * scale, apply.y + 11 * scale, "Apply");
  add_text(UiLayer::content, cancel.x + 12 * scale, cancel.y + 11 * scale, "Cancel");
  if (menu.selected_row() == GraphicsMenuRow::apply) {
    out.rectangles.push_back({UiLayer::focus, apply, focus});
  } else if (menu.selected_row() == GraphicsMenuRow::cancel) {
    out.rectangles.push_back({UiLayer::focus, cancel, focus});
  }
  return finish();
}

UiControl hit_test(const GraphicsMenuDrawList &list, float x, float y) noexcept {
  for (auto it = list.hit_targets.rbegin(); it != list.hit_targets.rend(); ++it) {
    if (it->enabled && contains(it->bounds, x, y)) {
      return it->control;
    }
  }
  return UiControl::none;
}

bool validate_graphics_menu_draw_list(const GraphicsMenuDrawList &list) noexcept {
  if (list.status != UiBuildStatus::ok || list.rectangles.size() > maximum_ui_rects ||
      list.texts.size() > maximum_ui_texts ||
      list.hit_targets.size() > maximum_ui_hit_targets) {
    return false;
  }
  std::size_t bytes = 0;
  for (const auto &command : list.rectangles) {
    if (!inside(command.bounds, list.target)) return false;
  }
  for (const auto &command : list.texts) {
    if (!inside(command.clip, list.target) || !std::isfinite(command.x) ||
        !std::isfinite(command.y)) return false;
    bytes += command.text.size();
    if (bytes > maximum_ui_text_bytes) return false;
  }
  for (const auto &target : list.hit_targets) {
    if (!inside(target.bounds, list.target)) return false;
  }
  return true;
}

} // namespace off::ui
