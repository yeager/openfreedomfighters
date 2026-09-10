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
