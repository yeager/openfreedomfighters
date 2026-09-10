#include "off/graphics/intro_renderer_workspace_layout.hpp"

#include "off/data/byte_reader.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace off::graphics {

IntroRendererWorkspaceLayout
parse_intro_renderer_workspace_layout(std::span<const std::byte> payload) {
  constexpr std::size_t word_size = sizeof(std::uint32_t);
  constexpr std::size_t header_words = 4U;
  if (payload.size() < header_words * word_size)
    throw std::runtime_error("Renderer workspace has no complete header");
  const data::ByteReader reader(payload);
  const auto prefix_words = static_cast<std::size_t>(reader.u32(0));
  const auto sixteen_words = static_cast<std::size_t>(reader.u32(4));
  const auto eight_words = static_cast<std::size_t>(reader.u32(8));
  if (prefix_words < header_words || eight_words % 2U != 0U)
    throw std::runtime_error("Renderer workspace header is invalid");
  if (prefix_words > std::numeric_limits<std::size_t>::max() - eight_words ||
      prefix_words + eight_words > std::numeric_limits<std::size_t>::max() - sixteen_words)
    throw std::runtime_error("Renderer workspace word extent overflows");
  const auto total_words = prefix_words + eight_words + sixteen_words;
  if (total_words > std::numeric_limits<std::size_t>::max() / word_size ||
      total_words * word_size != payload.size())
    throw std::runtime_error("Renderer workspace does not consume its payload exactly");
  return {
      .relocation_prefix_bytes = prefix_words * word_size,
      .eight_byte_slot_offset = prefix_words * word_size,
      .eight_byte_slot_count = eight_words / 2U,
      .sixteen_byte_slot_offset = (prefix_words + eight_words) * word_size,
      .sixteen_byte_slot_count = sixteen_words / 4U,
      .sixteen_byte_trailing_bytes = (sixteen_words % 4U) * word_size};
}

} // namespace off::graphics
