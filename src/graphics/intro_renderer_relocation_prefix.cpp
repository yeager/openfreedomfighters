#include "off/graphics/intro_renderer_relocation_prefix.hpp"

#include "off/data/byte_reader.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace off::graphics {

IntroRendererRelocationPrefix
parse_intro_renderer_relocation_prefix(std::span<const std::byte> payload) {
  constexpr std::size_t header_words = 4U;
  constexpr std::size_t word_size = sizeof(std::uint32_t);
  if (payload.size() < header_words * word_size)
    throw std::runtime_error("Renderer relocation prefix has no complete header");

  const data::ByteReader reader(payload);
  IntroRendererRelocationPrefix result;
  for (std::size_t index = 0; index < result.header.size(); ++index)
    result.header[index] = reader.u32(index * word_size);
  const auto prefix_words = static_cast<std::size_t>(result.header[0]);
  if (prefix_words < header_words ||
      prefix_words > std::numeric_limits<std::size_t>::max() / word_size ||
      prefix_words * word_size > payload.size())
    throw std::runtime_error("Renderer relocation prefix extent is invalid");
  result.byte_size = prefix_words * word_size;
  result.opaque_tail_offset = result.byte_size;

  auto cursor = header_words;
  while (cursor < prefix_words) {
    const auto chain_head = reader.u32(cursor++ * word_size);
    if (chain_head != 0U)
      throw std::runtime_error(
          "Renderer relocation group has an unsupported relation chain");
    if (cursor == prefix_words)
      throw std::runtime_error("Renderer relocation group has no references");
    IntroRendererRelocationGroup group;
    while (true) {
      if (cursor >= prefix_words)
        throw std::runtime_error(
            "Renderer relocation group has no terminal reference");
      const auto raw = reader.u32(cursor++ * word_size);
      group.references.push_back({raw});
      if ((raw & 1U) != 0U)
        break;
    }
    result.groups.push_back(std::move(group));
  }
  if (cursor != prefix_words)
    throw std::runtime_error("Renderer relocation prefix was not consumed exactly");
  return result;
}

} // namespace off::graphics
