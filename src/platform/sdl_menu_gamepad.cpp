#include "off/platform/sdl_menu_gamepad.hpp"

namespace off::platform {

namespace {
constexpr Sint16 menu_stick_threshold = 16000;

[[nodiscard]] std::int8_t stick_direction(Sint16 value) noexcept {
  if (value <= -menu_stick_threshold)
    return -1;
  if (value >= menu_stick_threshold)
    return 1;
  return 0;
}
} // namespace

SdlMenuGamepad::SdlMenuGamepad(SDL_WindowID window_id, bool focused)
    : window_id_(window_id), focused_(focused), focus_epoch_(SDL_GetTicksNS()) {
  select_available();
}
SdlMenuGamepad::~SdlMenuGamepad() {
  if (gamepad_)
    SDL_CloseGamepad(gamepad_);
}
void SdlMenuGamepad::snapshot_buttons() {
  held_.fill(false);
  if (gamepad_)
    for (std::size_t button = 0; button < held_.size(); ++button)
      held_[button] = SDL_GetGamepadButton(
          gamepad_, static_cast<SDL_GamepadButton>(button));
}
void SdlMenuGamepad::snapshot_axes() {
  left_stick_.fill(0);
  if (!gamepad_)
    return;
  left_stick_[0] = stick_direction(
      SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX));
  left_stick_[1] = stick_direction(
      SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY));
}
void SdlMenuGamepad::select_available(SDL_JoystickID excluded) {
  if (gamepad_)
    return;
  int count{};
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    if (ids[i] == excluded)
      continue;
    gamepad_ = SDL_OpenGamepad(ids[i]);
    if (gamepad_) {
      active_id_ = ids[i];
      focus_epoch_ = SDL_GetTicksNS();
      break;
    }
  }
  SDL_free(ids);
  snapshot_buttons();
  snapshot_axes();
}
std::optional<ui::GraphicsMenuKey>
SdlMenuGamepad::handle_event(const SDL_Event &event, bool menu_visible) {
  // A controller belongs to the application rather than an SDL window.  Do
  // not let a press made while this window is hidden or minimized become a
  // menu action when the application returns from the Steam overlay or a
  // suspend.  We intentionally resume only on INPUT_FOCUS_GAINED, not merely
  // a restore event, because restoring a background window does not grant it
  // input ownership.
  if ((event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
       event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
       event.type == SDL_EVENT_WINDOW_HIDDEN ||
       event.type == SDL_EVENT_WINDOW_MINIMIZED) &&
      event.window.windowID == window_id_) {
    focused_ = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
    focus_epoch_ = SDL_GetTicksNS();
    snapshot_buttons();
    snapshot_axes();
    return std::nullopt;
  }
  if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
      event.gdevice.which == active_id_) {
    if (gamepad_)
      SDL_CloseGamepad(gamepad_);
    gamepad_ = nullptr;
    active_id_ = 0;
    select_available(event.gdevice.which);
    return std::nullopt;
  }
  if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
    select_available();
    return std::nullopt;
  }
  if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    if (!gamepad_ || !SDL_GamepadConnected(gamepad_) ||
        event.gaxis.which != active_id_ || !focused_ ||
        event.gaxis.timestamp <= focus_epoch_ ||
        (event.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTX &&
         event.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTY))
      return std::nullopt;
    const std::size_t axis = event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX ? 0U : 1U;
    const auto direction = stick_direction(event.gaxis.value);
    if (direction == left_stick_[axis])
      return std::nullopt;
    left_stick_[axis] = direction;
    // Keep the baseline current while the overlay is hidden. A stick already
    // held for gameplay cannot navigate the menu just opened with Start.
    if (!menu_visible || direction == 0)
      return std::nullopt;
    if (axis == 0U)
      return direction < 0 ? ui::GraphicsMenuKey::left
                           : ui::GraphicsMenuKey::right;
    return direction < 0 ? ui::GraphicsMenuKey::up
                         : ui::GraphicsMenuKey::down;
  }
  if ((event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
       event.type != SDL_EVENT_GAMEPAD_BUTTON_UP) ||
      !gamepad_ || !SDL_GamepadConnected(gamepad_) ||
      event.gbutton.which != active_id_ ||
      event.gbutton.button >= SDL_GAMEPAD_BUTTON_COUNT)
    return std::nullopt;
  if (!focused_ || event.gbutton.timestamp <= focus_epoch_)
    return std::nullopt;
  const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  const bool was_held = held_[event.gbutton.button];
  held_[event.gbutton.button] = down;
  if (!down || was_held)
    return std::nullopt;
  if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
    return ui::GraphicsMenuKey::f10;
  if (!menu_visible)
    return std::nullopt;
  switch (event.gbutton.button) {
  case SDL_GAMEPAD_BUTTON_DPAD_UP:
    return ui::GraphicsMenuKey::up;
  case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    return ui::GraphicsMenuKey::down;
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
    return ui::GraphicsMenuKey::left;
  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
    return ui::GraphicsMenuKey::right;
  case SDL_GAMEPAD_BUTTON_SOUTH:
    return ui::GraphicsMenuKey::enter;
  case SDL_GAMEPAD_BUTTON_EAST:
    return ui::GraphicsMenuKey::escape;
  default:
    return std::nullopt;
  }
}

} // namespace off::platform
