#pragma once

#include "off/graphics/intro_renderer_payload_workspace.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace off::graphics {

// Records structural evidence from a real renderer payload after every
// reviewed relocation has resolved. It deliberately retains no renderer object
// and cannot admit a frame.
struct IntroRendererPayloadObservation {
  std::size_t relocation_groups{};
  std::size_t relocation_references{};
  std::size_t resolved_references{};
  std::size_t eight_byte_slots{};
  std::size_t sixteen_byte_slots{};
  std::size_t trailing_bytes{};
};

[[nodiscard]] IntroRendererPayloadObservation observe_intro_renderer_payload(
    std::span<const std::byte> payload,
    const std::function<std::optional<std::uint32_t>(std::uint32_t)>& resolver);

} // namespace off::graphics
