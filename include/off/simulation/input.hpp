#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace off::simulation {

enum class DigitalAction : std::uint8_t {
  move_forward,
  move_backward,
  move_left,
  move_right,
  fire,
  aim,
  interact,
  squad_command,
  count,
};

enum class AnalogAxis : std::uint8_t {
  move_x,
  move_y,
  look_x,
  look_y,
  count,
};

struct InputSnapshot {
  std::uint64_t tick{};
  std::uint64_t held{};
  std::uint64_t pressed{};
  std::uint64_t released{};
  std::array<std::int16_t, static_cast<std::size_t>(AnalogAxis::count)> axes{};
  auto operator<=>(const InputSnapshot &) const = default;

  [[nodiscard]] bool is_held(DigitalAction action) const noexcept;
  [[nodiscard]] bool was_pressed(DigitalAction action) const noexcept;
  [[nodiscard]] bool was_released(DigitalAction action) const noexcept;
};

class InputAccumulator final {
public:
  void set_action(DigitalAction action, bool down) noexcept;
  void set_axis(AnalogAxis axis, std::int16_t value) noexcept;
  void release_all() noexcept;

  // Returns all transitions accumulated since the previous snapshot. Held
  // actions and axes persist; pressed and released edges do not.
  [[nodiscard]] InputSnapshot take_snapshot(std::uint64_t tick) noexcept;

private:
  InputSnapshot pending_{};
};

} // namespace off::simulation
