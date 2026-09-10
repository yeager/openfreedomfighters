#include "off/graphics/intro_renderer_workspace_layout.hpp"

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
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class Function> void rejects(Function function, const char *message) {
  try { function(); } catch (const std::runtime_error &) { return; }
  check(false, message);
}
} // namespace

int main() {
  std::vector<std::byte> payload;
  for (const auto word : {7U, 5U, 4U, 0xffffffffU}) append_u32(payload, word);
  payload.resize((7U + 5U + 4U) * 4U);
  const auto layout = off::graphics::parse_intro_renderer_workspace_layout(payload);
  check(layout.relocation_prefix_bytes == 28U && layout.eight_byte_slot_offset == 28U &&
            layout.eight_byte_slot_count == 2U && layout.sixteen_byte_slot_offset == 44U &&
            layout.sixteen_byte_slot_count == 1U && layout.sixteen_byte_trailing_bytes == 4U,
        "derive both workspace ranges and preserve the final partial sixteen-byte slot bytes");
  auto odd_eight = payload; odd_eight[8] = std::byte{3};
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_workspace_layout(odd_eight)); },
          "reject an eight-byte workspace with an odd word count");
  auto incomplete = payload; incomplete.pop_back();
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_workspace_layout(incomplete)); },
          "reject a workspace whose framed extent differs from payload bytes");
  auto short_prefix = payload; short_prefix[0] = std::byte{3};
  rejects([&] { static_cast<void>(off::graphics::parse_intro_renderer_workspace_layout(short_prefix)); },
          "reject a prefix extent inside the header");
  std::cout << "intro renderer workspace layout tests passed\n";
}
