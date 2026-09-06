#include "off/ui/graphics_menu_pointer.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <utility>

namespace {
using namespace off::ui;
int failures{};
constexpr UiExtent extent{1280, 720};
const auto now = GraphicsClock::time_point{} + std::chrono::seconds(100);
void check(bool condition, const char *message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
GraphicsMenuSession opened() {
  GraphicsMenuSession menu{off::settings::GraphicsCapabilities{}};
  static_cast<void>(menu.handle_key(GraphicsMenuKey::f10, true, false));
  return menu;
}
std::pair<float, float> point(const GraphicsMenuSession &menu, UiControl control,
                              GraphicsClock::time_point time = now,
                              UiExtent target = extent) {
  const auto draw = build_graphics_menu_draw_list(menu, target, time);
  for (const auto &hit : draw.hit_targets)
    if (hit.control == control)
      return {hit.bounds.x + hit.bounds.width * 0.5F,
              hit.bounds.y + hit.bounds.height * 0.5F};
  check(false, "requested pointer control exists in fresh draw list");
  return {-1, -1};
}
GraphicsMenuEffect click(GraphicsMenuSession &menu, UiControl control,
                         GraphicsClock::time_point time = now,
                         UiExtent target = extent) {
  const auto [x, y] = point(menu, control, time, target);
  return dispatch_graphics_menu_pointer(menu, target, x, y, time);
}
GraphicsMenuSession confirming() {
  auto menu = opened();
  menu.draft().windowed_size = {1920, 1080};
  check(menu.request_apply().has_value(), "valid display change proposal");
  static_cast<void>(menu.acknowledge_apply(true, now));
  check(menu.phase() == GraphicsMenuPhase::confirming, "display change needs confirmation");
  return menu;
}
} // namespace

int main() {
  GraphicsMenuSession closed{off::settings::GraphicsCapabilities{}};
  check(dispatch_graphics_menu_pointer(closed, extent, 300, 300, now) ==
            GraphicsMenuEffect::none && closed.phase() == GraphicsMenuPhase::closed,
        "closed menu click neither opens nor quits");
  auto menu = opened();
  const auto original = menu.draft();
  for (const auto &[x, y] : {std::pair{-1.0F, 200.0F}, {200.0F, -1.0F},
                             {1280.0F, 300.0F}, {300.0F, 720.0F},
                             {std::numeric_limits<float>::infinity(), 200.0F},
                             {200.0F, std::numeric_limits<float>::quiet_NaN()}})
    check(dispatch_graphics_menu_pointer(menu, extent, x, y, now) ==
              GraphicsMenuEffect::none,
          "outside or nonfinite coordinates do not activate controls");
  check(dispatch_graphics_menu_pointer(menu, {0, 720}, 100, 100, now) ==
            GraphicsMenuEffect::none,
        "zero viewport cannot activate control");
  check(dispatch_graphics_menu_pointer(menu, extent, 300, 300, now, 0) ==
            GraphicsMenuEffect::none,
        "invalid UI scale cannot activate control");
  check(menu.draft() == original, "invalid clicks preserve draft");

  check(click(menu, UiControl::window_mode) == GraphicsMenuEffect::none,
        "option click is not an apply request");
  check(menu.selected_row() == GraphicsMenuRow::window_mode &&
            menu.draft().window_mode != original.window_mode,
        "option click selects its row and advances once");
  check(click(menu, UiControl::apply) == GraphicsMenuEffect::apply_requested,
        "Apply requests renderer transaction");
  check(menu.phase() == GraphicsMenuPhase::editing,
        "Apply dispatch does not acknowledge its own transaction");
  check(click(menu, UiControl::defaults) == GraphicsMenuEffect::none &&
            menu.draft() == off::settings::RequestedGraphicsSettings{},
        "Defaults restores requested defaults without applying");
  menu.draft().render_scale_percent = 150;
  check(click(menu, UiControl::cancel) == GraphicsMenuEffect::closed &&
            menu.draft() == menu.confirmed_requested(),
        "Cancel closes and discards unapplied edits");

  auto keep = confirming();
  check(click(keep, UiControl::keep) == GraphicsMenuEffect::commit_requested &&
            keep.phase() == GraphicsMenuPhase::closed &&
            keep.confirmed_requested().windowed_size ==
                off::settings::WindowSize{1920, 1080},
        "Keep before deadline commits display change");
  auto revert = confirming();
  const auto confirmed = revert.confirmed_requested();
  check(click(revert, UiControl::revert) == GraphicsMenuEffect::revert_requested &&
            revert.phase() == GraphicsMenuPhase::reverting &&
            revert.confirmed_requested() == confirmed,
        "Revert requests rollback without committing");
  check(dispatch_graphics_menu_pointer(revert, extent, 300, 300, now) ==
            GraphicsMenuEffect::none,
        "reverting has no active pointer controls");

  auto disabled = confirming();
  const auto almost_expired = *disabled.confirmation_deadline() -
                              std::chrono::microseconds(1);
  const auto disabled_draw = build_graphics_menu_draw_list(disabled, extent, almost_expired);
  bool disabled_keep = false;
  for (const auto &hit : disabled_draw.hit_targets)
    if (hit.control == UiControl::keep)
      disabled_keep = !hit.enabled;
  check(disabled_keep, "sub-millisecond remaining deadline disables Keep target");
  check(click(disabled, UiControl::keep, almost_expired) == GraphicsMenuEffect::none &&
            disabled.phase() == GraphicsMenuPhase::confirming,
        "disabled Keep cannot commit before timer expires");
  auto expired = confirming();
  const auto [keep_x, keep_y] = point(expired, UiControl::keep);
  check(dispatch_graphics_menu_pointer(expired, extent, keep_x, keep_y,
                                       *expired.confirmation_deadline()) ==
            GraphicsMenuEffect::revert_requested &&
            expired.phase() == GraphicsMenuPhase::reverting &&
            expired.confirmed_requested() == confirmed,
        "expired Keep location rolls back instead of stale-hit committing");
  auto expired_outside = confirming();
  check(dispatch_graphics_menu_pointer(expired_outside, extent, -1, -1,
                                       *expired_outside.confirmation_deadline()) ==
            GraphicsMenuEffect::revert_requested,
        "deadline rollback precedes even outside-pointer rejection");

  auto wide = opened();
  constexpr UiExtent ultrawide{2560, 1080};
  check(dispatch_graphics_menu_pointer(wide, ultrawide, 1, 540, now) ==
            GraphicsMenuEffect::none && wide.draft() == original,
        "letterbox area is not a menu control");
  check(click(wide, UiControl::window_size, now, ultrawide) == GraphicsMenuEffect::none &&
            wide.selected_row() == GraphicsMenuRow::window_size &&
            wide.draft().windowed_size != original.windowed_size,
        "physical-extent draw coordinates select correct letterboxed row");
  const auto mapped = map_graphics_menu_pointer_to_pixels(100, 100, extent, ultrawide);
  check(mapped && (*mapped)[0] == 200 && (*mapped)[1] == 150,
        "logical pointer coordinates scale independently to physical axes");
  check(!map_graphics_menu_pointer_to_pixels(1280, 0, extent, ultrawide) &&
            !map_graphics_menu_pointer_to_pixels(-1, 0, extent, ultrawide) &&
            !map_graphics_menu_pointer_to_pixels(0, 0, {0, 720}, ultrawide) &&
            !map_graphics_menu_pointer_to_pixels(0, 0, extent, {0, 1080}) &&
            !map_graphics_menu_pointer_to_pixels(
                std::numeric_limits<float>::quiet_NaN(), 0, extent, ultrawide),
        "mapping rejects invalid extents and outside/nonfinite coordinates");
  auto high_dpi = opened();
  const auto [px, py] = point(high_dpi, UiControl::window_mode, now, ultrawide);
  const auto physical = map_graphics_menu_pointer_to_pixels(px / 2, py / 1.5F,
                                                           extent, ultrawide);
  check(physical.has_value(), "logical high-DPI menu point maps successfully");
  if (physical)
    check(dispatch_graphics_menu_pointer(high_dpi, ultrawide, (*physical)[0],
                                         (*physical)[1], now) == GraphicsMenuEffect::none &&
              high_dpi.selected_row() == GraphicsMenuRow::window_mode &&
              high_dpi.draft().window_mode != original.window_mode,
          "mapped high-DPI click activates intended fresh target");
  return failures ? 1 : 0;
}
