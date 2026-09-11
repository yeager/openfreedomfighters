#include "off/graphics/intro_renderer_payload_workspace.hpp"
#include <stdexcept>
#include <utility>
namespace off::graphics {
IntroRendererPayloadWorkspace::IntroRendererPayloadWorkspace(std::vector<std::byte> bytes, IntroRendererRelocationPrefix prefix, IntroRendererWorkspaceLayout layout)
    : bytes_(std::move(bytes)), prefix_(std::move(prefix)), layout_(layout) {}
IntroRendererPayloadWorkspace IntroRendererPayloadWorkspace::from_prepared(IntroRendererRelocationPreparedPayload prepared) {
  const auto layout=parse_intro_renderer_workspace_layout(prepared.bytes);
  if (prepared.prefix.byte_size != layout.relocation_prefix_bytes || prepared.prefix.opaque_tail_offset != layout.relocation_prefix_bytes)
    throw std::runtime_error("Renderer payload prefix and workspace disagree");
  return {std::move(prepared.bytes), std::move(prepared.prefix), layout};
}
std::span<const std::byte> IntroRendererPayloadWorkspace::eight_byte_slot(std::size_t index) const {
  if (index >= layout_.eight_byte_slot_count) throw std::out_of_range("Renderer eight-byte workspace slot is absent");
  return std::span<const std::byte>{bytes_}.subspan(layout_.eight_byte_slot_offset + index * 8U, 8U);
}
std::span<const std::byte> IntroRendererPayloadWorkspace::sixteen_byte_slot(std::size_t index) const {
  if (index >= layout_.sixteen_byte_slot_count) throw std::out_of_range("Renderer sixteen-byte workspace slot is absent");
  return std::span<const std::byte>{bytes_}.subspan(layout_.sixteen_byte_slot_offset + index * 16U, 16U);
}
std::span<const std::byte> IntroRendererPayloadWorkspace::sixteen_byte_trailing_bytes() const noexcept {
  const auto offset=layout_.sixteen_byte_slot_offset + layout_.sixteen_byte_slot_count * 16U;
  return std::span<const std::byte>{bytes_}.subspan(offset, layout_.sixteen_byte_trailing_bytes);
}
} // namespace off::graphics
