#include "off/ui/graphics_menu_draw.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
bool has_text(const off::ui::GraphicsMenuDrawList &list,
              const std::string &value) {
  for (const auto &text : list.texts)
    if (text.text == value)
      return true;
  return false;
}
const off::ui::UiTextCommand *
find_text(const off::ui::GraphicsMenuDrawList &list, const std::string &value) {
  for (const auto &text : list.texts)
    if (text.text == value)
      return &text;
  return nullptr;
}
bool near(float actual, float expected) {
  return std::abs(actual - expected) < 0.01F;
}
} // namespace

int main() {
  off::settings::GraphicsCapabilities capabilities;
  off::ui::GraphicsMenuSession menu{capabilities};
  const auto now = off::ui::GraphicsClock::time_point{};
  const auto closed =
      off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now);
  check(closed.status == off::ui::UiBuildStatus::ok &&
            closed.rectangles.empty(),
        "closed menu has no overlay commands");
  check(off::ui::build_graphics_menu_draw_list(menu, {0, 720}, now).status ==
            off::ui::UiBuildStatus::invalid_viewport,
        "zero swapchain extent is rejected");
  check(off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now, 0.0F)
                .status == off::ui::UiBuildStatus::invalid_scale,
        "unsafe UI scale is rejected");

  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  menu.draft().profile = off::Mode::modern;
  menu.draft().window_mode = off::settings::WindowMode::borderless_desktop;
  menu.draft().windowed_size = {1920, 1080};
  const off::ui::UiExtent sizes[]{
      {640, 360}, {1280, 720}, {1920, 1080}, {3440, 1440}};
  for (const auto size : sizes) {
    const auto list = off::ui::build_graphics_menu_draw_list(menu, size, now);
    check(list.status == off::ui::UiBuildStatus::ok &&
              off::ui::validate_graphics_menu_draw_list(list) &&
              list.hit_targets.size() == 8 && has_text(list, "Modern") &&
              has_text(list, "Borderless desktop") &&
              has_text(list, "1920 x 1080") && has_text(list, "VSync") &&
              has_text(list, "100%") && has_text(list, "Native") &&
              has_text(list, "Reference") && has_text(list, "Apply"),
          "layout is bounded and truthful at every target size");
  }
  const auto reference =
      off::ui::build_graphics_menu_draw_list(menu, {640, 480}, now);
  const auto *title = find_text(reference, "GRAPHICS SETTINGS");
  const auto *profile = find_text(reference, "Profile");
  const auto *profile_value = find_text(reference, "Modern");
  const auto *shadows = find_text(reference, "Shadows");
  const auto *action = find_text(reference, "Apply");
  check(title != nullptr && near(title->x, 60.0F) && near(title->y, 155.0F) &&
            profile != nullptr && near(profile->x, 60.0F) &&
            near(profile->y, 185.0F) && profile_value != nullptr &&
            near(profile_value->x, 400.0F) && near(profile_value->y, 185.0F) &&
            shadows != nullptr && near(shadows->y, 293.0F) &&
            action != nullptr && near(action->x, 60.0F) &&
            near(action->y, 400.0F),
        "640x480 uses recovered retail anchors and seven-slot rhythm");
  check(reference.rectangles.back().layer == off::ui::UiLayer::focus &&
            near(reference.rectangles.back().bounds.x, 44.0F) &&
            near(reference.rectangles.back().bounds.y, 183.0F) &&
            near(reference.rectangles.back().bounds.width, 16.0F) &&
            near(reference.rectangles.back().bounds.height, 16.0F),
        "focus uses only the recovered 16x16 state marker geometry");

  const auto widescreen =
      off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now);
  const auto *wide_title = find_text(widescreen, "GRAPHICS SETTINGS");
  check(wide_title != nullptr && near(wide_title->x, 250.0F) &&
            near(wide_title->y, 232.5F),
        "widescreen centers an aspect-preserving 640x480 authored viewport");
  const auto first =
      off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now);
  check(first == off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now),
        "identical state produces deterministic commands");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::down, true, false));
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::window_mode &&
            menu.draft().window_mode == off::settings::WindowMode::windowed,
        "keyboard focus edits the selected real setting");
  for (int i = 0; i < 3; ++i)
    static_cast<void>(
        menu.handle_key(off::ui::GraphicsMenuKey::down, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::apply &&
            menu.handle_key(off::ui::GraphicsMenuKey::enter, true, false) ==
                off::ui::GraphicsMenuEffect::apply_requested,
        "Apply yields a transaction request");
  const auto apply_state =
      off::ui::build_graphics_menu_draw_list(menu, {640, 480}, now);
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::down, true, false));
  const auto back_state =
      off::ui::build_graphics_menu_draw_list(menu, {640, 480}, now);
  const auto *apply_text = find_text(apply_state, "Apply");
  const auto *back_text = find_text(back_state, "Back");
  check(apply_text != nullptr && back_text != nullptr &&
            near(apply_text->x, 60.0F) && near(back_text->x, 60.0F) &&
            near(apply_text->y, 400.0F) && near(back_text->y, 400.0F) &&
            apply_state.hit_targets.back().control ==
                off::ui::UiControl::apply &&
            back_state.hit_targets.back().control == off::ui::UiControl::cancel,
        "Apply and Back are mutually selected states at the retail anchor");
  static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::up, true, false));

  menu.draft().windowed_size = {1920, 1080};
  check(menu.request_apply().has_value(), "valid edits start apply");
  static_cast<void>(menu.acknowledge_apply(true, now));
  const auto confirmation = off::ui::build_graphics_menu_draw_list(
      menu, {1280, 720}, now + std::chrono::milliseconds{1});
  check(confirmation.hit_targets.size() == 2 &&
            has_text(confirmation, "Reverting in 15 seconds") &&
            off::ui::hit_test(confirmation,
                              confirmation.hit_targets[0].bounds.x + 1,
                              confirmation.hit_targets[0].bounds.y + 1) ==
                off::ui::UiControl::keep,
        "confirmation replaces editable controls with Keep/Revert");
  const auto expired = off::ui::build_graphics_menu_draw_list(
      menu, {1280, 720}, now + std::chrono::seconds{15});
  check(has_text(expired, "Reverting in 0 seconds") &&
            !expired.hit_targets[0].enabled &&
            off::ui::hit_test(expired, expired.hit_targets[0].bounds.x + 1,
                              expired.hit_targets[0].bounds.y + 1) ==
                off::ui::UiControl::none,
        "expired display confirmation cannot be kept");
  const auto atlas = off::ui::make_diagnostic_ascii_atlas();
  check(atlas.extent == off::ui::UiExtent{128, 96} &&
            atlas.alpha.size() == 128U * 96U,
        "diagnostic atlas dimensions are deterministic");
  std::size_t opaque_pixels = 0;
  for (const auto alpha : atlas.alpha)
    opaque_pixels += alpha == 255 ? 1U : 0U;
  check(opaque_pixels > 1000 && atlas.alpha[80U * 128U + 120U] == 255,
        "generated Spleen glyphs and the solid UI texel are present");
  return failures == 0 ? 0 : 1;
}
