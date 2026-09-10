#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::graphics {

struct IntroRendererTaggedReference {
  std::uint32_t raw{};
  [[nodiscard]] std::uint32_t address() const noexcept { return raw & ~7U; }
  [[nodiscard]] std::uint8_t tag() const noexcept {
    return static_cast<std::uint8_t>(raw & 7U);
  }
  [[nodiscard]] bool terminal() const noexcept { return (raw & 1U) != 0U; }
};

struct IntroRendererRelocationGroup {
  std::vector<IntroRendererTaggedReference> references;
};

// Source-only relocation prefix. The remainder of the renderer payload stays
// opaque, and parsing this prefix does not relocate or materialize resources.
struct IntroRendererRelocationPrefix {
  std::array<std::uint32_t, 4> header{};
  std::size_t byte_size{};
  std::size_t opaque_tail_offset{};
  std::vector<IntroRendererRelocationGroup> groups;
};

[[nodiscard]] IntroRendererRelocationPrefix
parse_intro_renderer_relocation_prefix(std::span<const std::byte> payload);

} // namespace off::graphics
