#include "off/graphics/intro_renderer_relocation_prefix.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
std::uint32_t read_u32(const std::vector<std::byte> &bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8)
    value |= std::to_integer<std::uint32_t>(bytes[offset++]) << shift;
  return value;
}
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
template <class Function> void rejects(Function function, const char *message) {
  try {
    function();
  } catch (const std::runtime_error &) {
    return;
  }
  check(false, message);
}
} // namespace

int main() {
  std::vector<std::byte> payload;
  for (const auto word : {9U, 40U, 12U, 0xffffffffU, 0U, 0x100U,
                          0x209U, 0U, 0x311U, 0xdeadbeefU})
    append_u32(payload, word);
  const auto parsed =
      off::graphics::parse_intro_renderer_relocation_prefix(payload);
  check(parsed.byte_size == 36U && parsed.opaque_tail_offset == 36U &&
            parsed.groups.size() == 2U &&
            parsed.groups[0].references.size() == 2U &&
            parsed.groups[0].references[0].address() == 0x100U &&
            parsed.groups[0].references[0].raw == 0x100U &&
            parsed.groups[0].references[0].word_offset == 5U &&
            parsed.groups[0].references[1].tag() == 1U &&
            parsed.groups[1].references[0].terminal() &&
            parsed.header[1] == 40U,
        "parse exact zero-head relocation groups and preserve the opaque tail");

  rejects([] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix({})); },
          "reject a missing prefix header");
  for (std::size_t size = 1; size < 16; ++size) {
    std::vector<std::byte> truncated(size);
    rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(truncated)); },
            "reject every truncated prefix header");
  }
  for (std::uint32_t words = 0; words < 4; ++words) {
    std::vector<std::byte> short_extent;
    for (const auto word : {words, 0U, 0U, 0U}) append_u32(short_extent, word);
    rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(short_extent)); },
            "reject an extent inside the fixed header");
  }
  std::vector<std::byte> empty_prefix;
  for (const auto word : {4U, 1U, 2U, 3U}) append_u32(empty_prefix, word);
  empty_prefix.push_back(std::byte{0xaa});
  const auto empty = off::graphics::parse_intro_renderer_relocation_prefix(empty_prefix);
  check(empty.groups.empty() && empty.byte_size == 16U && empty.opaque_tail_offset == 16U,
        "accept an empty prefix and a non-word-aligned opaque tail");
  std::vector<std::byte> tagged;
  for (const auto word : {7U, 0U, 0U, 0U, 0U, 0U, 0x127U}) append_u32(tagged, word);
  const auto tags = off::graphics::parse_intro_renderer_relocation_prefix(tagged);
  check(tags.groups[0].references.size() == 2U && tags.groups[0].references[0].raw == 0U &&
            tags.groups[0].references[0].address() == 0U && tags.groups[0].references[0].tag() == 0U &&
            tags.groups[0].references[1].raw == 0x127U && tags.groups[0].references[1].address() == 0x120U &&
            tags.groups[0].references[1].tag() == 7U && tags.groups[0].references[1].terminal(),
        "preserve zero addresses, raw values, address masks, and all tag bits");
  std::vector<std::uint32_t> lookups;
  const auto prepared = off::graphics::prepare_intro_renderer_relocation_payload(
      tagged, [&](std::uint32_t lookup) -> std::optional<std::uint32_t> {
        lookups.push_back(lookup);
        return 0x880U;
      });
  check(lookups == std::vector<std::uint32_t>{0x40000180U} &&
            read_u32(prepared.bytes, 5U * 4U) == 0U &&
            read_u32(prepared.bytes, 6U * 4U) == 0x887U &&
            read_u32(tagged, 6U * 4U) == 0x127U &&
            prepared.prefix.groups[0].references[1].raw == 0x127U,
        "relocate an owned copy with the retail bias while preserving source metadata and tags");
  rejects([&] {
    static_cast<void>(off::graphics::prepare_intro_renderer_relocation_payload(
        tagged, [](std::uint32_t) -> std::optional<std::uint32_t> { return std::nullopt; }));
  }, "reject an unresolved renderer reference");
  rejects([&] {
    static_cast<void>(off::graphics::prepare_intro_renderer_relocation_payload(
        tagged, [](std::uint32_t) -> std::optional<std::uint32_t> { return 3U; }));
  }, "reject a renderer relocation result that collides with tag bits");
  rejects([&] {
    static_cast<void>(off::graphics::prepare_intro_renderer_relocation_payload(tagged, {}));
  }, "reject a missing renderer relocation service");
  std::vector<std::byte> outside_domain;
  for (const auto word : {6U, 0U, 0U, 0U, 0U, 0x3fffffa1U})
    append_u32(outside_domain, word);
  rejects([&] {
    static_cast<void>(off::graphics::prepare_intro_renderer_relocation_payload(
        outside_domain, [](std::uint32_t) -> std::optional<std::uint32_t> { return 8U; }));
  }, "reject a source address whose bias collides with the domain marker");
  auto oversized = payload;
  oversized[0] = std::byte{20};
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(oversized)); },
          "reject a prefix beyond the payload");
  auto chained = payload;
  chained[16] = std::byte{4};
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(chained)); },
          "reject unrecovered relation-chain serialization");
  auto unterminated = payload;
  unterminated[0] = std::byte{7};
  unterminated[24] = std::byte{8};
  rejects(
      [&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(unterminated)); },
      "reject a group without a terminal tagged reference");
  std::vector<std::byte> head_only;
  for (const auto word : {5U, 0U, 0U, 0U, 0U}) append_u32(head_only, word);
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_relocation_prefix(head_only)); },
          "reject a final group head with no reference");
  std::cout << "intro renderer relocation prefix tests passed\n";
}
