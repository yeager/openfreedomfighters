#include "off/ui/graphics_menu_draw.hpp"

#include <chrono>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
bool has_text(const off::ui::GraphicsMenuDrawList &list,
              const std::string &value) {
  for (const auto &text : list.texts) if (text.text == value) return true;
  return false;
}
} // namespace

int main() {
  off::settings::GraphicsCapabilities capabilities;
  off::ui::GraphicsMenuSession menu{capabilities};
  const auto now = off::ui::GraphicsClock::time_point{};
  const auto closed = off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now);
  check(closed.status == off::ui::UiBuildStatus::ok && closed.rectangles.empty(),
        "closed menu has no overlay commands");
  check(off::ui::build_graphics_menu_draw_list(menu, {0, 720}, now).status ==
            off::ui::UiBuildStatus::invalid_viewport,
        "zero swapchain extent is rejected");
  check(off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now, 0.0F).status ==
            off::ui::UiBuildStatus::invalid_scale,
        "unsafe UI scale is rejected");

  static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  menu.draft().profile = off::Mode::modern;
  menu.draft().window_mode = off::settings::WindowMode::borderless_desktop;
  menu.draft().windowed_size = {1920, 1080};
  const off::ui::UiExtent sizes[]{{640, 360}, {1280, 720}, {1920, 1080}, {3440, 1440}};
  for (const auto size : sizes) {
    const auto list = off::ui::build_graphics_menu_draw_list(menu, size, now);
    check(list.status == off::ui::UiBuildStatus::ok &&
              off::ui::validate_graphics_menu_draw_list(list) &&
              list.hit_targets.size() == 6 && has_text(list, "Modern") &&
              has_text(list, "Borderless desktop") &&
              has_text(list, "1920 x 1080") && has_text(list, "VSync"),
          "layout is bounded and truthful at every target size");
  }
  const auto first = off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now);
  check(first == off::ui::build_graphics_menu_draw_list(menu, {1280, 720}, now),
        "identical state produces deterministic commands");
  static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::down, true, false));
  static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::window_mode &&
            menu.draft().window_mode == off::settings::WindowMode::windowed,
        "keyboard focus edits the selected real setting");
  for (int i = 0; i < 3; ++i)
    static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::down, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::apply &&
            menu.handle_key(off::ui::GraphicsMenuKey::enter, true, false) ==
                off::ui::GraphicsMenuEffect::apply_requested,
        "Apply yields a transaction request");

  menu.draft().windowed_size = {1920, 1080};
  check(menu.request_apply().has_value(), "valid edits start apply");
  static_cast<void>(menu.acknowledge_apply(true, now));
  const auto confirmation = off::ui::build_graphics_menu_draw_list(
      menu, {1280, 720}, now + std::chrono::milliseconds{1});
  check(confirmation.hit_targets.size() == 2 &&
            has_text(confirmation, "Reverting in 15 seconds") &&
            off::ui::hit_test(confirmation,
                confirmation.hit_targets[0].bounds.x + 1,
                confirmation.hit_targets[0].bounds.y + 1) == off::ui::UiControl::keep,
        "confirmation replaces editable controls with Keep/Revert");
  const auto expired = off::ui::build_graphics_menu_draw_list(
      menu, {1280, 720}, now + std::chrono::seconds{15});
  check(has_text(expired, "Reverting in 0 seconds") &&
            !expired.hit_targets[0].enabled &&
            off::ui::hit_test(expired, expired.hit_targets[0].bounds.x + 1,
                              expired.hit_targets[0].bounds.y + 1) == off::ui::UiControl::none,
        "expired display confirmation cannot be kept");
  const auto atlas = off::ui::make_diagnostic_ascii_atlas();
  check(atlas.extent == off::ui::UiExtent{128, 96} && atlas.alpha.size() == 128U * 96U,
        "diagnostic atlas dimensions are deterministic");
  return failures == 0 ? 0 : 1;
}
