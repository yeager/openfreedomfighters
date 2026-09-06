#include "off/ui/graphics_menu_pointer.hpp"

#include <cmath>

namespace off::ui {

std::optional<std::array<float, 2>>
map_graphics_menu_pointer_to_pixels(float x, float y, UiExtent logical,
                                    UiExtent physical) {
  if (logical.width == 0 || logical.height == 0 || physical.width == 0 ||
      physical.height == 0 || !std::isfinite(x) || !std::isfinite(y) || x < 0 ||
      y < 0 || double(x) >= logical.width || double(y) >= logical.height)
    return std::nullopt;
  return std::array<float, 2>{
      static_cast<float>(double(x) * physical.width / logical.width),
      static_cast<float>(double(y) * physical.height / logical.height)};
}

GraphicsMenuEffect dispatch_graphics_menu_pointer(GraphicsMenuSession &menu,
                                                  UiExtent physical_target,
                                                  float x, float y,
                                                  GraphicsClock::time_point now,
                                                  float ui_scale) {
  if (menu.tick(now) == GraphicsMenuEffect::revert_requested)
    return GraphicsMenuEffect::revert_requested;
  if (menu.phase() == GraphicsMenuPhase::closed || !std::isfinite(x) ||
      !std::isfinite(y) || x < 0 || y < 0 ||
      double(x) >= physical_target.width || double(y) >= physical_target.height)
    return GraphicsMenuEffect::none;
  const auto list =
      build_graphics_menu_draw_list(menu, physical_target, now, ui_scale);
  if (list.status != UiBuildStatus::ok ||
      !validate_graphics_menu_draw_list(list))
    return GraphicsMenuEffect::none;
  const auto control = hit_test(list, x, y);
  if (menu.phase() == GraphicsMenuPhase::confirming) {
    if (control == UiControl::keep)
      return menu.confirm();
    if (control == UiControl::revert)
      return menu.cancel_or_revert();
    return GraphicsMenuEffect::none;
  }
  std::optional<GraphicsMenuRow> row;
  switch (control) {
  case UiControl::profile:
    row = GraphicsMenuRow::profile;
    break;
  case UiControl::window_mode:
    row = GraphicsMenuRow::window_mode;
    break;
  case UiControl::window_size:
    row = GraphicsMenuRow::window_size;
    break;
  case UiControl::present_mode:
    row = GraphicsMenuRow::present_mode;
    break;
  case UiControl::render_scale:
    row = GraphicsMenuRow::render_scale;
    break;
  case UiControl::upscaler:
    row = GraphicsMenuRow::upscaler;
    break;
  case UiControl::shadows:
    row = GraphicsMenuRow::shadows;
    break;
  case UiControl::apply:
    row = GraphicsMenuRow::apply;
    break;
  case UiControl::cancel:
    row = GraphicsMenuRow::cancel;
    break;
  case UiControl::defaults:
    row = GraphicsMenuRow::defaults;
    break;
  default:
    return GraphicsMenuEffect::none;
  }
  if (!menu.select_row(*row))
    return GraphicsMenuEffect::none;
  const bool action = control == UiControl::apply ||
                      control == UiControl::cancel ||
                      control == UiControl::defaults;
  return menu.handle_key(
      action ? GraphicsMenuKey::enter : GraphicsMenuKey::right, true, false);
}

} // namespace off::ui
