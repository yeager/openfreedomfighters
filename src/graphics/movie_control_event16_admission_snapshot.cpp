#include "off/graphics/movie_control_event16_admission_snapshot.hpp"

#include <algorithm>
#include <stdexcept>

namespace off::graphics {
MovieControlEvent16ManagerSnapshot MovieControlEvent16ManagerSnapshot::capture(
    const runtime::OrdinaryComponentManager& manager,
    const runtime::ComponentRecord& component, std::uint64_t component_handle,
    std::int32_t scene_integer, bool frame_paused,
    std::optional<std::uint64_t> frame_filter) {
  if (!component_handle || manager.failed() || manager.traversing())
    throw std::runtime_error("MovieControl event16 snapshot requires a stable live manager");
  if (!component.constructed() || component.removed() || !component.identity() ||
      std::uint64_t{*component.identity()} + 1U != component_handle)
    throw std::runtime_error("MovieControl event16 snapshot component relation is not live");
  const auto enrolled_count = static_cast<std::size_t>(std::count(
      manager.retained().begin(), manager.retained().end(), component_handle));
  if (enrolled_count > 1)
    throw std::runtime_error("MovieControl event16 snapshot found duplicate manager enrollment");
  const auto& state = component.state();
  return {component_handle, scene_integer, true,
          enrolled_count == 1 && (state.admitted & 0x10U) != 0,
          frame_paused, frame_filter,
          (state.requested & 1U) == 0 || (state.status & 4U) != 0};
}

MovieControlEvent16Services MovieControlEvent16ManagerSnapshot::bind(
    DirectServices direct) const {
  return {
      [value = component_is_live] { return value; },
      [value = event16_enrolled] { return value; },
      [value = paused] { return value; },
      [value = component_filter] { return value; },
      [value = phase_one_completed] { return value; },
      [value = scene_integer] { return value; },
      std::move(direct.prepare_sequence_resources),
      std::move(direct.send_cut_sequence_start),
      std::move(direct.send_group_state_requests)};
}
}  // namespace off::graphics
