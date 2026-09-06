#include "off/platform/sdl_menu_gamepad.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
using Key = off::ui::GraphicsMenuKey;
int failures{};
void check(bool condition, const char *message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << " (SDL: " << SDL_GetError() << ")\n";
  }
}
struct VirtualPad {
  SDL_JoystickID id{};
  SDL_Joystick *joystick{};
  bool attach(const char *name) {
    SDL_VirtualJoystickDesc description{};
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.name = name;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    description.button_mask = (Uint32{1} << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    id = SDL_AttachVirtualJoystick(&description);
    if (id)
      joystick = SDL_OpenJoystick(id);
    return id != 0;
  }
  void detach() {
    if (joystick) {
      SDL_CloseJoystick(joystick);
      joystick = nullptr;
    }
    if (id) {
      check(SDL_DetachVirtualJoystick(id), "detach virtual gamepad");
      id = 0;
    }
  }
  ~VirtualPad() { detach(); }
};
SDL_Event button_event(SDL_JoystickID id, SDL_GamepadButton button, bool down,
                       Uint64 timestamp = 0) {
  SDL_Event event{};
  event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
  event.gbutton.which = id;
  event.gbutton.button = static_cast<Uint8>(button);
  event.gbutton.down = down;
  event.gbutton.timestamp = timestamp ? timestamp : SDL_GetTicksNS() + 1;
  return event;
}
void focus(off::platform::SdlMenuGamepad &input, SDL_WindowID window,
           bool gained) {
  SDL_Event event{};
  event.type = gained ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
  event.window.windowID = window;
  check(!input.handle_event(event, true), "focus transition emits no menu key");
  SDL_Delay(1);
}
std::vector<Key> drain(off::platform::SdlMenuGamepad &input, bool visible) {
  std::vector<Key> keys;
  SDL_Event event{};
  while (SDL_PollEvent(&event))
    if (auto key = input.handle_event(event, visible))
      keys.push_back(*key);
  return keys;
}
std::vector<Key> edge(off::platform::SdlMenuGamepad &input, VirtualPad &pad,
                      SDL_GamepadButton button, bool down, bool visible) {
  SDL_Delay(1);
  check(SDL_SetJoystickVirtualButton(pad.joystick, button, down),
        "set actual virtual gamepad button");
  SDL_UpdateJoysticks();
  return drain(input, visible);
}
void expect(const std::vector<Key> &actual, Key expected, const char *message) {
  check(actual == std::vector<Key>{expected}, message);
}
int run() {
  int existing_count{};
  auto *existing = SDL_GetGamepads(&existing_count);
  const bool enumerated = existing != nullptr;
  SDL_free(existing);
  if (!enumerated) {
    check(false, "enumerate gamepads before isolated test");
    return 1;
  }
  if (existing_count != 0) {
    std::cerr << "SKIP: virtual controller test requires no pre-existing gamepads\n";
    return 77;
  }
  VirtualPad first;
  if (!first.attach("OpenFreedomFighters test controller one")) {
    std::string error = SDL_GetError();
    std::transform(error.begin(), error.end(), error.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (error.find("not supported") != std::string::npos ||
        error.find("unsupported") != std::string::npos) {
      std::cerr << "SKIP: SDL virtual joystick attachment unsupported: "
                << SDL_GetError() << '\n';
      return 77;
    }
    check(false, "attach initial virtual gamepad");
    return 1;
  }
  check(first.joystick != nullptr, "open virtual joystick");
  check(SDL_IsGamepad(first.id), "virtual device has SDL gamepad mapping");
  if (!first.joystick || !SDL_IsGamepad(first.id))
    return 1;
  SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
  constexpr SDL_WindowID window = 42;
  off::platform::SdlMenuGamepad input(window, true);
  check(input.active_id() == first.id, "enumerate controller already attached");
  static_cast<void>(drain(input, false));
  SDL_Delay(1);
  expect(edge(input, first, SDL_GAMEPAD_BUTTON_START, true, false), Key::f10,
         "Start opens hidden menu");
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_START, true), true),
        "duplicate Start down is not another edge");
  check(edge(input, first, SDL_GAMEPAD_BUTTON_START, false, true).empty(),
        "Start release emits nothing");
  expect(edge(input, first, SDL_GAMEPAD_BUTTON_START, true, true), Key::f10,
         "Start may close visible menu after release");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_START, false, true));
  for (const auto &[button, key] : std::vector<std::pair<SDL_GamepadButton, Key>>{
           {SDL_GAMEPAD_BUTTON_DPAD_UP, Key::up},
           {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Key::down},
           {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Key::left},
           {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Key::right},
           {SDL_GAMEPAD_BUTTON_SOUTH, Key::enter},
           {SDL_GAMEPAD_BUTTON_EAST, Key::escape}}) {
    expect(edge(input, first, button, true, true), key, "visible menu button mapping");
    check(edge(input, first, button, false, true).empty(), "release is not activation");
  }
  check(edge(input, first, SDL_GAMEPAD_BUTTON_EAST, true, false).empty(),
        "East cannot request quit from hidden menu");
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_EAST, true), true),
        "opening menu while East held does not synthesize activation");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_EAST, false, true));
  check(edge(input, first, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true, false).empty(),
        "hidden menu ignores dpad");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false, false));
  check(!input.handle_event(button_event(0, SDL_GAMEPAD_BUTTON_START, true), false),
        "unknown controller cannot toggle menu");
  auto invalid = button_event(first.id, SDL_GAMEPAD_BUTTON_START, true);
  invalid.gbutton.button = 255;
  check(!input.handle_event(invalid, false), "out-of-range button is ignored");

  focus(input, window + 1, false);
  expect(edge(input, first, SDL_GAMEPAD_BUTTON_START, true, false), Key::f10,
         "other window focus loss does not suppress input");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_START, false, false));
  focus(input, window, false);
  check(edge(input, first, SDL_GAMEPAD_BUTTON_START, true, false).empty(),
        "unfocused Start emits nothing");
  focus(input, window, true);
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_START, true), false),
        "focus gain snapshots actually held button");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_START, false, false));
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_START, true, 1), false),
        "stale pre-focus button down ignored");
  expect(edge(input, first, SDL_GAMEPAD_BUTTON_START, true, false), Key::f10,
         "fresh press after focus baseline and stale down activates");
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_START, false, 1), false),
        "stale release ignored");
  check(!input.handle_event(button_event(first.id, SDL_GAMEPAD_BUTTON_START, true), false),
        "stale release did not clear held baseline");
  static_cast<void>(edge(input, first, SDL_GAMEPAD_BUTTON_START, false, false));

  off::ui::GraphicsMenuSession menu{off::settings::GraphicsCapabilities{}};
  auto dispatch = [&](SDL_GamepadButton button) {
    auto keys = edge(input, first, button, true,
                     menu.phase() != off::ui::GraphicsMenuPhase::closed);
    off::ui::GraphicsMenuEffect effect = off::ui::GraphicsMenuEffect::none;
    for (auto key : keys)
      effect = menu.handle_key(key, true, false);
    static_cast<void>(edge(input, first, button, false, true));
    return effect;
  };
  check(dispatch(SDL_GAMEPAD_BUTTON_START) == off::ui::GraphicsMenuEffect::opened,
        "actual Start edge opens GraphicsMenuSession");
  menu.draft().windowed_size = {1920, 1080};
  check(menu.request_apply().has_value(), "display change proposal accepted");
  static_cast<void>(menu.acknowledge_apply(true, off::ui::GraphicsClock::now()));
  check(menu.phase() == off::ui::GraphicsMenuPhase::confirming,
        "display change awaits confirmation");
  check(dispatch(SDL_GAMEPAD_BUTTON_EAST) == off::ui::GraphicsMenuEffect::revert_requested,
        "East requests rollback during display confirmation");
  check(menu.phase() == off::ui::GraphicsMenuPhase::reverting,
        "rollback must await renderer acknowledgement");
  static_cast<void>(menu.acknowledge_revert(true));

  VirtualPad second;
  check(second.attach("OpenFreedomFighters test controller two"), "attach second gamepad");
  if (!second.id || !second.joystick)
    return 1;
  static_cast<void>(drain(input, false));
  check(input.active_id() == first.id, "hotplug does not steal active controller");
  check(edge(input, second, SDL_GAMEPAD_BUTTON_START, true, false).empty(),
        "inactive controller buttons ignored");
  const auto removed = first.id;
  first.detach();
  check(drain(input, false).empty(), "removal does not activate menu");
  check(input.active_id() == second.id, "removal selects remaining controller");
  check(!input.handle_event(button_event(second.id, SDL_GAMEPAD_BUTTON_START, true), false),
        "replacement snapshots held Start without activation");
  check(!input.handle_event(button_event(removed, SDL_GAMEPAD_BUTTON_START, true), false),
        "removed device cannot activate menu");
  static_cast<void>(edge(input, second, SDL_GAMEPAD_BUTTON_START, false, false));
  expect(edge(input, second, SDL_GAMEPAD_BUTTON_START, true, false), Key::f10,
         "replacement activates after release and new press");
  second.detach();
  static_cast<void>(drain(input, false));
  check(input.active_id() == 0, "last removal leaves no active controller");
  VirtualPad third;
  check(third.attach("OpenFreedomFighters test controller three"), "attach after empty enumeration");
  static_cast<void>(drain(input, false));
  check(third.id && input.active_id() == third.id, "hotplug recovers from no controller");
  return failures ? 1 : 0;
}
} // namespace

int main() {
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::cerr << "FAIL: SDL initialization: " << SDL_GetError() << '\n';
    return 1;
  }
  const int result = run();
  SDL_Quit();
  return result;
}
