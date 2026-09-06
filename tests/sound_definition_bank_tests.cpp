#include "off/data/sound_definition_bank.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using off::data::SoundDefinitionBank;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& operation) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, "expected sound bank rejection");
}
void word(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = std::byte((value >> (i * 8)) & 255U);
}
std::vector<std::byte> fixture() {
  std::vector<std::byte> bytes(80, std::byte{0xaa});
  // Independently authored fixture. There is no assumed/stripped bank header.
  word(bytes, 19, 1); word(bytes, 23, 61); word(bytes, 27, 0xdeadbeef);
  word(bytes, 31, std::bit_cast<std::uint32_t>(3.25F));
  bytes[61] = std::byte{'a'}; bytes[62] = std::byte{'/'};
  bytes[63] = std::byte{0xff}; bytes[64] = std::byte{0};
  return bytes;
}
}
int main() {
  try {
    auto bytes = fixture();
    auto bank = SoundDefinitionBank::parse(bytes, bytes.size());
    check(bank.size() == 80 && !bank.simple_definition(0), "complete bank base and zero null reference");
    bytes.clear();
    const auto definition = bank.simple_definition(19, 3);
    check(definition && definition->definition_offset == 19 && definition->identifier_offset == 61 &&
          definition->resource_link == 0xdeadbeef && definition->duration == 3.25F &&
          definition->duration_bits == std::bit_cast<std::uint32_t>(3.25F),
          "unaligned complete-image record and distinct identifier/resource link domains");
    check(definition->logical_identifier.size() == 3 && definition->logical_identifier[0] == 'a' &&
          static_cast<unsigned char>(definition->logical_identifier[2]) == 255,
          "logical name preserves raw bytes without filesystem/UTF-8 interpretation");
    const auto surviving = SoundDefinitionBank::parse(fixture(), 80).simple_definition(19);
    check(surviving->logical_identifier == definition->logical_identifier, "returned name owns its storage");
    rejects([&] { (void)bank.simple_definition(19, 2); });
    rejects([&] { (void)bank.simple_definition(65); });
    rejects([&] { (void)bank.simple_definition(UINT32_MAX); });
    rejects([] { (void)SoundDefinitionBank::parse(fixture(), 79); });
    const auto empty = SoundDefinitionBank::parse({}, 0);
    check(!empty.simple_definition(0), "empty image has no fabricated header requirements or nonzero records");
    rejects([&] { (void)empty.simple_definition(1); });
    for (auto type : {0U, 2U, UINT32_MAX}) {
      auto bad = fixture(); word(bad, 19, type);
      rejects([&] { (void)SoundDefinitionBank::parse(bad, 80).simple_definition(19); });
    }
    for (auto offset : {80U, UINT32_MAX}) {
      auto bad = fixture(); word(bad, 23, offset);
      rejects([&] { (void)SoundDefinitionBank::parse(bad, 80).simple_definition(19); });
    }
    auto unterminated = fixture(); word(unterminated, 23, 79);
    rejects([&] { (void)SoundDefinitionBank::parse(unterminated, 80).simple_definition(19); });
    auto empty_name = fixture(); word(empty_name, 23, 64);
    check(SoundDefinitionBank::parse(empty_name, 80).simple_definition(19, 0)->logical_identifier.empty(),
          "terminated empty logical name is not given an invented prohibition");
    for (auto bits : {0xbf800000U, 0x7f800000U, 0xff800000U, 0x7fc00001U}) {
      auto bad = fixture(); word(bad, 31, bits);
      rejects([&] { (void)SoundDefinitionBank::parse(bad, 80).simple_definition(19); });
    }
    for (auto bits : {0U, 0x80000000U, 1U, 0x7f7fffffU}) {
      auto valid = fixture(); word(valid, 31, bits);
      const auto value = SoundDefinitionBank::parse(valid, 80).simple_definition(19);
      check(value->duration_bits == bits && std::bit_cast<std::uint32_t>(value->duration) == bits,
            "accepted duration preserves original bits including negative zero/subnormal");
    }
    std::cout << "Complete SND image, bounded type-one lookup, identifier ownership and duration bits verified.\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
