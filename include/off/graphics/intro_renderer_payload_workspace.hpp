#pragma once
#include "off/graphics/intro_renderer_relocation_prefix.hpp"
#include "off/graphics/intro_renderer_workspace_layout.hpp"
#include <cstddef>
#include <span>
#include <vector>
namespace off::graphics {
class IntroRendererPayloadWorkspace final {
public:
  [[nodiscard]] static IntroRendererPayloadWorkspace from_prepared(IntroRendererRelocationPreparedPayload prepared);
  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }
  [[nodiscard]] const IntroRendererRelocationPrefix &prefix() const noexcept { return prefix_; }
  [[nodiscard]] const IntroRendererWorkspaceLayout &layout() const noexcept { return layout_; }
  [[nodiscard]] std::span<const std::byte> eight_byte_slot(std::size_t index) const;
  [[nodiscard]] std::span<const std::byte> sixteen_byte_slot(std::size_t index) const;
  [[nodiscard]] std::span<const std::byte> sixteen_byte_trailing_bytes() const noexcept;
private:
  IntroRendererPayloadWorkspace(std::vector<std::byte> bytes, IntroRendererRelocationPrefix prefix, IntroRendererWorkspaceLayout layout);
  std::vector<std::byte> bytes_;
  IntroRendererRelocationPrefix prefix_;
  IntroRendererWorkspaceLayout layout_;
};
} // namespace off::graphics
