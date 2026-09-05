#include "off/simulation/input.hpp"

namespace off::simulation {
namespace {

constexpr std::uint64_t bit(DigitalAction action) noexcept {
  const auto index = static_cast<std::uint8_t>(action);
  return index < 64 ? std::uint64_t{1} << index : 0;
}

} // namespace

bool InputSnapshot::is_held(DigitalAction action) const noexcept {
  return (held & bit(action)) != 0;
}

bool InputSnapshot::was_pressed(DigitalAction action) const noexcept {
  return (pressed & bit(action)) != 0;
}

bool InputSnapshot::was_released(DigitalAction action) const noexcept {
  return (released & bit(action)) != 0;
}

void InputAccumulator::set_action(DigitalAction action, bool down) noexcept {
  const auto mask = bit(action);
  const auto was_down = (pending_.held & mask) != 0;
  if (down == was_down)
    return;
  if (down) {
    pending_.held |= mask;
    pending_.pressed |= mask;
  } else {
    pending_.held &= ~mask;
    pending_.released |= mask;
  }
}

void InputAccumulator::set_axis(AnalogAxis axis, std::int16_t value) noexcept {
  const auto index = static_cast<std::size_t>(axis);
  if (index < pending_.axes.size())
    pending_.axes[index] = value;
}

void InputAccumulator::release_all() noexcept {
  pending_.released |= pending_.held;
  pending_.held = 0;
  pending_.axes.fill(0);
}

InputSnapshot InputAccumulator::take_snapshot(std::uint64_t tick) noexcept {
  pending_.tick = tick;
  const auto result = pending_;
  pending_.pressed = 0;
  pending_.released = 0;
  return result;
}

} // namespace off::simulation
