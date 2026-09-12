#include "off/platform/sdl_menu_keyboard.hpp"

namespace off::platform {

std::optional<ui::GraphicsMenuKey>
translate_menu_keyboard_event(const SDL_Event &event, SDL_WindowID window_id,
                              bool focused) noexcept {
  if (!focused || (event.type != SDL_EVENT_KEY_DOWN &&
                   event.type != SDL_EVENT_KEY_UP) ||
      event.key.windowID != window_id)
    return std::nullopt;

  switch (event.key.key) {
  case SDLK_F10:
    return ui::GraphicsMenuKey::f10;
  case SDLK_ESCAPE:
    return ui::GraphicsMenuKey::escape;
  case SDLK_UP:
    return ui::GraphicsMenuKey::up;
  case SDLK_DOWN:
    return ui::GraphicsMenuKey::down;
  case SDLK_LEFT:
    return ui::GraphicsMenuKey::left;
  case SDLK_RIGHT:
    return ui::GraphicsMenuKey::right;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    return ui::GraphicsMenuKey::enter;
  case SDLK_SPACE:
    return ui::GraphicsMenuKey::space;
  default:
    return std::nullopt;
  }
}

} // namespace off::platform
