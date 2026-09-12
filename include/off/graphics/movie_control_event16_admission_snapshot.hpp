#pragma once

#include "off/graphics/intro_controller_initialization.hpp"
#include "off/runtime/ordinary_components.hpp"

#include <cstdint>
#include <functional>
#include <optional>

namespace off::graphics {

// An immutable capture of the inputs the ordinary-component manager has
// already established for one MovieControl event-16 pass. It deliberately has
// no manager traversal, scheduling, lifecycle, or receiver behavior.
struct MovieControlEvent16ManagerSnapshot {
  std::uint64_t component_handle{};
  std::int32_t scene_integer{};
  bool component_is_live{};
  bool event16_enrolled{};
  bool paused{};
  std::optional<std::uint64_t> component_filter;
  bool phase_one_completed{};

  struct DirectServices {
    std::function<void()> prepare_sequence_resources;
    std::function<void(std::uint64_t)> send_cut_sequence_start;
    std::function<void(std::uint64_t)> send_group_state_requests;
  };

  // Requires a stable, non-failed manager and a constructed, non-removed
  // record whose common-construction identity proves component_handle. A
  // retained handle is enrollment only when the ordinary admission bit is live.
  // Phase-one completion follows the manager's own event-16 gate: either the
  // phase-one request bit is absent or status bit 0x4 is present.
  [[nodiscard]] static MovieControlEvent16ManagerSnapshot capture(
      const runtime::OrdinaryComponentManager& manager,
      const runtime::ComponentRecord& component, std::uint64_t component_handle,
      std::int32_t scene_integer, bool paused,
      std::optional<std::uint64_t> component_filter);

  // Produces only already-captured gate callbacks. This does not deliver an
  // event; MovieControlFirstUpdate retains every deadline/receiver decision.
  [[nodiscard]] MovieControlEvent16Services bind(DirectServices direct) const;
};

}  // namespace off::graphics
