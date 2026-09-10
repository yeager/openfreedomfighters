#pragma once

#include <cstddef>
#include <span>

namespace off::graphics {

// Source-backed, mutable workspace ranges established by the ordinary
// renderer-payload constructor. Their slots have no recovered render grammar.
struct IntroRendererWorkspaceLayout {
  std::size_t relocation_prefix_bytes{};
  std::size_t eight_byte_slot_offset{}, eight_byte_slot_count{};
  std::size_t sixteen_byte_slot_offset{}, sixteen_byte_slot_count{};
  std::size_t sixteen_byte_trailing_bytes{};
};

[[nodiscard]] IntroRendererWorkspaceLayout
parse_intro_renderer_workspace_layout(std::span<const std::byte> payload);

} // namespace off::graphics
