#include "off/platform/sdl_menu_keyboard.hpp"

#include <iostream>

namespace {
using Key = off::ui::GraphicsMenuKey;
int failures{};

void check(bool condition, const char *message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

SDL_Event key_event(SDL_WindowID window, SDL_Keycode key, bool down = true) {
  SDL_Event event{};
  event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  event.key.windowID = window;
  event.key.key = key;
  return event;
}

void expect(const SDL_Event &event, SDL_WindowID window, bool focused,
            std::optional<Key> expected, const char *message) {
  check(off::platform::translate_menu_keyboard_event(event, window, focused) ==
            expected,
        message);
}
} // namespace

int main() {
  constexpr SDL_WindowID runtime_window = 41;
  constexpr SDL_WindowID foreign_window = 42;
  expect(key_event(runtime_window, SDLK_F10), runtime_window, true, Key::f10,
         "focused runtime F10 opens the overlay");
  expect(key_event(runtime_window, SDLK_KP_ENTER), runtime_window, true,
         Key::enter, "keypad Enter is available to the focused overlay");
  expect(key_event(runtime_window, SDLK_SPACE, false), runtime_window, true,
         Key::space, "key releases retain their mapping for symmetric routing");
  expect(key_event(foreign_window, SDLK_F10), runtime_window, true, std::nullopt,
         "foreign SDL windows cannot toggle the runtime overlay");
  expect(key_event(foreign_window, SDLK_ESCAPE), runtime_window, true,
         std::nullopt, "foreign SDL windows cannot request runtime quit");
  expect(key_event(runtime_window, SDLK_F10), runtime_window, false,
         std::nullopt, "unfocused runtime ignores overlay keyboard input");
  expect(key_event(runtime_window, SDLK_A), runtime_window, true, std::nullopt,
         "unmapped gameplay keys remain outside project overlay routing");
  SDL_Event unrelated{};
  unrelated.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  expect(unrelated, runtime_window, true, std::nullopt,
         "non-keyboard events are not reinterpreted as keyboard input");
  return failures == 0 ? 0 : 1;
}
