#pragma once

#include "off/ui/graphics_menu.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace off::platform {

// Project overlay keys only.  This deliberately admits events from the
// runtime's focused window, so a second SDL window cannot open, alter, or
// dismiss the graphics overlay.
[[nodiscard]] std::optional<ui::GraphicsMenuKey>
translate_menu_keyboard_event(const SDL_Event &event, SDL_WindowID window_id,
                              bool focused) noexcept;

} // namespace off::platform
