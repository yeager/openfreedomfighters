#include "off/graphics/intro_renderer_relocation_prefix.hpp"

#include "off/data/byte_reader.hpp"

#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace off::graphics {
namespace {
void write_u32(std::vector<std::byte> &bytes, std::size_t offset,
               std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
}
} // namespace

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
      group.references.push_back({raw, cursor - 1U});
      if ((raw & 1U) != 0U)
        break;
    }
    result.groups.push_back(std::move(group));
  }
  if (cursor != prefix_words)
    throw std::runtime_error("Renderer relocation prefix was not consumed exactly");
  return result;
}

IntroRendererRelocationPreparedPayload
prepare_intro_renderer_relocation_payload(
    std::span<const std::byte> payload,
    const std::function<std::optional<std::uint32_t>(std::uint32_t)> &resolver) {
  if (!resolver)
    throw std::runtime_error("Renderer relocation requires a resolver");
  IntroRendererRelocationPreparedPayload result{
      .bytes = {payload.begin(), payload.end()},
      .prefix = parse_intro_renderer_relocation_prefix(payload)};
  constexpr std::uint32_t bias = 0x60U;
  constexpr std::uint32_t domain_marker = 0x40000000U;
  std::vector<std::tuple<std::size_t, std::uint32_t, std::uint8_t>> resolved_words;
  for (const auto &group : result.prefix.groups) {
    for (const auto &reference : group.references) {
      if (reference.address() == 0U)
        continue;
      if (reference.address() >= domain_marker ||
          reference.address() > domain_marker - 1U - bias)
        throw std::runtime_error("Renderer relocation reference is outside its source domain");
      const auto resolved = resolver((reference.address() + bias) | domain_marker);
      if (!resolved)
        throw std::runtime_error("Renderer relocation reference did not resolve");
      if ((*resolved & 7U) != 0U)
        throw std::runtime_error("Renderer relocation result is not tag-aligned");
      resolved_words.emplace_back(reference.word_offset, *resolved,
                                  reference.tag());
    }
  }
  for (const auto &[word_offset, resolved, tag] : resolved_words)
    write_u32(result.bytes, word_offset * sizeof(std::uint32_t), resolved | tag);
  return result;
}

} // namespace off::graphics
