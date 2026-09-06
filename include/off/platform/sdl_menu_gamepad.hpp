#pragma once

#include "off/ui/graphics_menu.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <optional>

namespace off::platform {

// Project menu mapping, not recovered gameplay controls. Caller owns the SDL
// gamepad subsystem and invokes this object only on the SDL event thread.
class SdlMenuGamepad final {
public:
  explicit SdlMenuGamepad(SDL_WindowID window_id, bool focused);
  ~SdlMenuGamepad();
  SdlMenuGamepad(const SdlMenuGamepad &) = delete;
  SdlMenuGamepad &operator=(const SdlMenuGamepad &) = delete;
  [[nodiscard]] SDL_JoystickID active_id() const noexcept { return active_id_; }
  [[nodiscard]] std::optional<ui::GraphicsMenuKey>
  handle_event(const SDL_Event &event, bool menu_visible);

private:
  void select_available(SDL_JoystickID excluded = 0);
  void snapshot_buttons();
  SDL_WindowID window_id_;
  bool focused_;
  SDL_Gamepad *gamepad_{};
  SDL_JoystickID active_id_{};
  Uint64 focus_epoch_{};
  std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> held_{};
};

} // namespace off::platform
