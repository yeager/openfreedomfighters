#include "off/ui/graphics_menu_draw.hpp"
#include "off/ui/detail/spleen_ascii_rows.hpp"
#include "off/ui/project_localization.hpp"

#include <algorithm>
#include <cmath>

namespace off::ui {
namespace {

constexpr UiColor dim{0, 0, 0, 170};
constexpr UiColor panel{20, 25, 35, 245};
constexpr UiColor focus{232, 176, 55, 255};
constexpr UiColor white{240, 243, 248, 255};
constexpr UiColor muted{166, 174, 187, 255};
constexpr float reference_width = 640.0F;
constexpr float reference_height = 480.0F;
constexpr std::size_t visible_option_slots = 7;
using PlatformLocales = std::span<const std::string_view>;

bool contains(const UiRect &rect, float x, float y) {
  return x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
         y < rect.y + rect.height;
}

bool inside(const UiRect &rect, UiExtent target) {
  return std::isfinite(rect.x) && std::isfinite(rect.y) &&
         std::isfinite(rect.width) && std::isfinite(rect.height) &&
         rect.width > 0.0F && rect.height > 0.0F && rect.x >= 0.0F &&
         rect.y >= 0.0F &&
         rect.x + rect.width <= static_cast<float>(target.width) &&
         rect.y + rect.height <= static_cast<float>(target.height);
}

bool normalized(const UiRect &rect) {
  return std::isfinite(rect.x) && std::isfinite(rect.y) &&
         std::isfinite(rect.width) && std::isfinite(rect.height) &&
         rect.width > 0.0F && rect.height > 0.0F && rect.x >= 0.0F &&
         rect.y >= 0.0F && rect.x + rect.width <= 1.0F &&
         rect.y + rect.height <= 1.0F;
}

bool valid_layer(UiLayer layer) { return layer <= UiLayer::modal; }

std::string localized(l10n::MessageId id, std::string_view explicit_locale,
                      PlatformLocales platform_locales) {
  const auto text =
      l10n::f10_catalog().resolve(id, explicit_locale, platform_locales);
  return text ? std::string{*text} : std::string{};
}

std::string profile_name(const settings::RequestedGraphicsSettings &value,
                         std::string_view explicit_locale,
                         PlatformLocales platform_locales) {
  return localized(value.profile == Mode::original
                       ? l10n::MessageId::original
                       : (value.modern_plus ? l10n::MessageId::modern_plus
                                            : l10n::MessageId::modern),
                   explicit_locale, platform_locales);
}

std::string upscaler_name(settings::Upscaler value,
                          std::string_view explicit_locale,
                          PlatformLocales platform_locales) {
  switch (value) {
  case settings::Upscaler::native:
    return localized(l10n::MessageId::native, explicit_locale, platform_locales);
  case settings::Upscaler::temporal:
    return localized(l10n::MessageId::temporal, explicit_locale,
                     platform_locales);
  case settings::Upscaler::dlss:
    return localized(l10n::MessageId::dlss, explicit_locale, platform_locales);
  }
  return {};
}

std::string shadow_name(settings::ShadowQuality value,
                        std::string_view explicit_locale,
                        PlatformLocales platform_locales) {
  switch (value) {
  case settings::ShadowQuality::reference:
    return localized(l10n::MessageId::reference, explicit_locale,
                     platform_locales);
  case settings::ShadowQuality::high:
    return localized(l10n::MessageId::high, explicit_locale, platform_locales);
  case settings::ShadowQuality::ultra:
    return localized(l10n::MessageId::ultra, explicit_locale, platform_locales);
  }
  return {};
}

std::string window_name(settings::WindowMode mode,
                        std::string_view explicit_locale,
                        PlatformLocales platform_locales) {
  return localized(mode == settings::WindowMode::borderless_desktop
                       ? l10n::MessageId::borderless_desktop
                       : l10n::MessageId::windowed,
                   explicit_locale, platform_locales);
}

std::string present_name(settings::PresentMode mode,
                         std::string_view explicit_locale,
                         PlatformLocales platform_locales) {
  switch (mode) {
  case settings::PresentMode::vsync:
    return localized(l10n::MessageId::vsync, explicit_locale, platform_locales);
  case settings::PresentMode::mailbox:
    return localized(l10n::MessageId::mailbox, explicit_locale,
                     platform_locales);
  case settings::PresentMode::immediate:
    return localized(l10n::MessageId::immediate, explicit_locale,
                     platform_locales);
  }
  return {};
}

} // namespace

DiagnosticAsciiAtlas make_diagnostic_ascii_atlas() {
  DiagnosticAsciiAtlas atlas;
  atlas.alpha.assign(
      static_cast<std::size_t>(atlas.extent.width) * atlas.extent.height, 0);
  for (std::size_t glyph = 0; glyph < detail::spleen_ascii_rows.size();
       ++glyph) {
    const auto cell_x =
        static_cast<std::uint32_t>(glyph % atlas.columns) * atlas.glyph_width;
    const auto cell_y =
        static_cast<std::uint32_t>(glyph / atlas.columns) * atlas.glyph_height;
    for (std::uint32_t y = 0; y < atlas.glyph_height; ++y) {
      const auto bits = detail::spleen_ascii_rows[glyph][y];
      for (std::uint32_t x = 0; x < atlas.glyph_width; ++x) {
        if ((bits & (0x80U >> x)) != 0) {
          const auto offset =
              static_cast<std::size_t>(cell_y + y) * atlas.extent.width +
              cell_x + x;
          atlas.alpha[offset] = 255;
        }
      }
    }
  }
  // The unused 96th cell supplies an opaque white rectangle texel.
  atlas.alpha[static_cast<std::size_t>(5U * atlas.glyph_height) *
                  atlas.extent.width +
              15U * atlas.glyph_width] = 255;
  return atlas;
}

GraphicsMenuDrawList
build_graphics_menu_draw_list(const GraphicsMenuSession &menu, UiExtent target,
                              GraphicsClock::time_point now, float scale,
                              std::string_view explicit_locale,
                              PlatformLocales platform_locales) {
  GraphicsMenuDrawList out;
  out.target = target;
  out.ui_scale = scale;
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
  const float fit =
      std::min(width / reference_width, height / reference_height);
  const float physical_scale = std::min(fit * scale, fit);
  const float viewport_width = reference_width * physical_scale;
  const float viewport_height = reference_height * physical_scale;
  const float origin_x = (width - viewport_width) * 0.5F;
  const float origin_y = (height - viewport_height) * 0.5F;
  const UiRect panel_rect{origin_x, origin_y, viewport_width, viewport_height};
  const auto point_x = [&](float value) {
    return origin_x + value * physical_scale;
  };
  const auto point_y = [&](float value) {
    return origin_y + value * physical_scale;
  };
  const auto reference_rect = [&](float x, float y, float w, float h) {
    return UiRect{point_x(x), point_y(y), w * physical_scale,
                  h * physical_scale};
  };
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
      out.ui_scale = scale;
      out.status = UiBuildStatus::capacity_exceeded;
    }
    return out;
  };
  if (!add_text(UiLayer::content, point_x(60.0F), point_y(155.0F),
                localized(l10n::MessageId::graphics_settings, explicit_locale,
                          platform_locales))) {
    out.rectangles.clear();
    out.texts.clear();
    return out;
  }

  if (menu.phase() == GraphicsMenuPhase::confirming) {
    auto seconds = 0LL;
    if (const auto deadline = menu.confirmation_deadline()) {
      const auto milliseconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              *deadline > now ? *deadline - now
                              : GraphicsClock::duration::zero())
              .count();
      seconds = (milliseconds + 999) / 1000;
    }
    add_text(UiLayer::modal, point_x(60.0F), point_y(185.0F),
             localized(l10n::MessageId::keep_display_settings, explicit_locale,
                       platform_locales));
    const auto countdown = l10n::f10_catalog().format_seconds(
        l10n::MessageId::reverting_in_seconds, static_cast<unsigned>(seconds),
        explicit_locale, platform_locales);
    add_text(UiLayer::modal, point_x(60.0F), point_y(203.0F),
             countdown ? std::string{*countdown} : std::string{});
    const UiRect keep = reference_rect(60.0F, 400.0F, 150.0F, 18.0F);
    const UiRect revert = reference_rect(230.0F, 400.0F, 150.0F, 18.0F);
    out.hit_targets.push_back({keep, UiControl::keep, seconds > 0});
    out.hit_targets.push_back({revert, UiControl::revert, true});
    add_text(
        UiLayer::modal, keep.x, keep.y,
        localized(l10n::MessageId::keep, explicit_locale, platform_locales));
    add_text(
        UiLayer::modal, revert.x, revert.y,
        localized(l10n::MessageId::revert, explicit_locale, platform_locales));
    return finish();
  }

  if (menu.phase() == GraphicsMenuPhase::applying ||
      menu.phase() == GraphicsMenuPhase::reverting) {
    add_text(UiLayer::modal, point_x(60.0F), point_y(185.0F),
             menu.phase() == GraphicsMenuPhase::applying
                 ? localized(l10n::MessageId::applying_settings,
                             explicit_locale, platform_locales)
                 : localized(l10n::MessageId::restoring_settings,
                             explicit_locale, platform_locales));
    return finish();
  }

  const auto &draft = menu.draft();
  const std::array labels{
      localized(l10n::MessageId::profile, explicit_locale, platform_locales),
      localized(l10n::MessageId::window_mode, explicit_locale, platform_locales),
      localized(l10n::MessageId::resolution, explicit_locale, platform_locales),
      localized(l10n::MessageId::present_mode, explicit_locale,
                platform_locales),
      localized(l10n::MessageId::render_scale, explicit_locale,
                platform_locales),
      localized(l10n::MessageId::upscaler, explicit_locale, platform_locales),
      localized(l10n::MessageId::shadows, explicit_locale, platform_locales)};
  const std::array values{
      profile_name(draft, explicit_locale, platform_locales),
      window_name(draft.window_mode, explicit_locale, platform_locales),
      std::to_string(draft.windowed_size.width) + " x " +
          std::to_string(draft.windowed_size.height),
      present_name(draft.present_mode, explicit_locale, platform_locales),
      std::to_string(draft.render_scale_percent) + "%",
      upscaler_name(draft.upscaler, explicit_locale, platform_locales),
      shadow_name(draft.shadow_quality, explicit_locale, platform_locales)};
  const std::array controls{UiControl::profile,      UiControl::window_mode,
                            UiControl::window_size,  UiControl::present_mode,
                            UiControl::render_scale, UiControl::upscaler,
                            UiControl::shadows};
  static_assert(labels.size() == visible_option_slots);
  for (std::size_t i = 0; i < visible_option_slots; ++i) {
    const float reference_y = 185.0F + static_cast<float>(i) * 18.0F;
    // The broad pointer target is a portable accessibility policy. Retail
    // evidence supplies the anchors and rhythm, but not the final hit box.
    const UiRect bounds =
        reference_rect(60.0F, reference_y - 2.0F, 520.0F, 18.0F);
    out.hit_targets.push_back({bounds, controls[i], true});
    add_text(UiLayer::content, point_x(60.0F), point_y(reference_y), labels[i],
             muted);
    add_text(UiLayer::content, point_x(400.0F), point_y(reference_y),
             values[i]);
    if (static_cast<std::size_t>(menu.selected_row()) == i) {
      // This project-authored rectangle is only a diagnostic focus marker.
      // Recovered row chrome consists of two simultaneously gated picture
      // instances; neither is a behavior-proven focused/normal alternative.
      out.rectangles.push_back(
          {UiLayer::focus,
           reference_rect(44.0F, reference_y - 2.0F, 16.0F, 16.0F), focus});
    }
  }
  // Project diagnostic extension: expose all actions for pointer access.
  // Only the first anchor comes from retail layout evidence; these three
  // simultaneously accessible controls are not a recovered retail layout.
  constexpr std::array actions{UiControl::apply, UiControl::cancel,
                               UiControl::defaults};
  constexpr std::array action_rows{GraphicsMenuRow::apply,
                                   GraphicsMenuRow::cancel,
                                   GraphicsMenuRow::defaults};
  const std::array action_labels{
      localized(l10n::MessageId::apply, explicit_locale, platform_locales),
      localized(l10n::MessageId::back, explicit_locale, platform_locales),
      localized(l10n::MessageId::defaults, explicit_locale, platform_locales)};
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const float x = 60.0F + static_cast<float>(i) * 180.0F;
    out.hit_targets.push_back(
        {reference_rect(x, 398.0F, 160.0F, 18.0F), actions[i], true});
    add_text(UiLayer::content, point_x(x), point_y(400.0F), action_labels[i]);
    if (menu.selected_row() == action_rows[i])
      out.rectangles.push_back({UiLayer::focus,
                                reference_rect(x - 16.0F, 398.0F, 16.0F, 16.0F),
                                focus});
  }
  return finish();
}

GraphicsMenuDrawList
build_graphics_menu_draw_list(const GraphicsMenuSession &menu, UiExtent target,
                              GraphicsClock::time_point now, float scale,
                              std::string_view explicit_locale,
                              std::string_view platform_locale) {
  const std::array<std::string_view, 1> platform_locales{{platform_locale}};
  return build_graphics_menu_draw_list(menu, target, now, scale,
                                       explicit_locale, platform_locales);
}

UiControl hit_test(const GraphicsMenuDrawList &list, float x,
                   float y) noexcept {
  for (auto it = list.hit_targets.rbegin(); it != list.hit_targets.rend();
       ++it) {
    if (it->enabled && contains(it->bounds, x, y)) {
      return it->control;
    }
  }
  return UiControl::none;
}

bool validate_graphics_menu_draw_list(
    const GraphicsMenuDrawList &list) noexcept {
  if (list.status != UiBuildStatus::ok ||
      list.rectangles.size() > maximum_ui_rects ||
      list.textures.size() > maximum_ui_texture_commands ||
      list.texts.size() > maximum_ui_texts ||
      list.hit_targets.size() > maximum_ui_hit_targets) {
    return false;
  }
  std::size_t bytes = 0;
  for (const auto &command : list.rectangles) {
    if (!valid_layer(command.layer) || !inside(command.bounds, list.target))
      return false;
  }
  for (const auto &command : list.textures) {
    if (!valid_layer(command.layer) ||
        command.texture_role > RetailUiTextureRole::arrow_down ||
        !inside(command.bounds, list.target) || !normalized(command.source))
      return false;
  }
  for (const auto &command : list.texts) {
    if (!valid_layer(command.layer) || !inside(command.clip, list.target) ||
        !std::isfinite(command.x) || !std::isfinite(command.y))
      return false;
    bytes += command.text.size();
    if (bytes > maximum_ui_text_bytes)
      return false;
  }
  for (const auto &target : list.hit_targets) {
    if (!inside(target.bounds, list.target))
      return false;
  }
  return true;
}

} // namespace off::ui
