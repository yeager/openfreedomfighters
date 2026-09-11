#include "off/graphics/intro_renderer_payload_observation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

namespace {
void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
void check(bool value, const char* message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
} // namespace

int main() {
  std::vector<std::byte> payload;
  for (const auto word : {6U, 4U, 2U, 0U, 0U, 0x101U}) append_u32(payload, word);
  payload.resize(48U);
  std::vector<std::uint32_t> lookups;
  const auto observed = off::graphics::observe_intro_renderer_payload(
      payload, [&](std::uint32_t key) -> std::optional<std::uint32_t> {
        lookups.push_back(key); return 0x880U;
      });
  check(lookups == std::vector<std::uint32_t>{0x40000160U} &&
            observed.relocation_groups == 1U && observed.relocation_references == 1U &&
            observed.resolved_references == 1U && observed.eight_byte_slots == 1U &&
            observed.sixteen_byte_slots == 1U && observed.trailing_bytes == 0U,
        "renderer observation relocates only source-backed references and retains workspace structure");
  std::cout << "intro renderer payload observation tests passed\n";
}
