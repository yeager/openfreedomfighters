#include "off/graphics/intro_renderer_workspace_layout.hpp"
#include "off/graphics/intro_renderer_payload_workspace.hpp"

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
  try { function(); } catch (const std::exception &) { return; }
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
  std::vector<std::byte> prepared_payload;
  for (const auto word : {6U, 4U, 2U, 0U, 0U, 1U}) append_u32(prepared_payload, word);
  prepared_payload.resize(48U);
  for (std::size_t index=24U;index<prepared_payload.size();++index)
    prepared_payload[index]=static_cast<std::byte>(index);
  const auto workspace=off::graphics::IntroRendererPayloadWorkspace::from_prepared(
      off::graphics::prepare_intro_renderer_relocation_payload(prepared_payload,{}));
  check(workspace.bytes().size()==48U && workspace.prefix().groups.size()==1U &&
            workspace.eight_byte_slot(0U).size()==8U && workspace.eight_byte_slot(0U)[0]==std::byte{24} &&
            workspace.sixteen_byte_slot(0U).size()==16U && workspace.sixteen_byte_slot(0U)[0]==std::byte{32} &&
            workspace.sixteen_byte_trailing_bytes().empty(),
        "retain prepared renderer bytes through bounded opaque workspace slots");
  rejects([&] { static_cast<void>(workspace.eight_byte_slot(1U)); },
          "reject an absent eight-byte workspace slot");
  std::cout << "intro renderer workspace layout tests passed\n";
}
