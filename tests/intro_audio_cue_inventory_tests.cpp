#include "off/graphics/intro_audio_cue_inventory.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(const bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template <class F> void rejects(F&& action, const char* message) {
  try { action(); } catch (const std::invalid_argument&) { return; }
  throw std::runtime_error(message);
}
void put(std::vector<std::byte>& bytes, const std::size_t offset, const std::uint32_t value) {
  for (std::size_t index = 0; index < 4U; ++index)
    bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
}
off::data::AudioBankHeader header() {
  std::vector<std::byte> bytes(72U);
  put(bytes, 0U, 64U); put(bytes, 4U, 72U); put(bytes, 8U, 3U); put(bytes, 12U, 4U);
  put(bytes, 16U, 6U); put(bytes, 24U, 0x1000U); put(bytes, 28U, 44100U);
  put(bytes, 32U, 16U); put(bytes, 36U, 8U); put(bytes, 40U, 4U); put(bytes, 44U, 2U);
  put(bytes, 48U, 1U); put(bytes, 52U, 8U); put(bytes, 56U, 4U); put(bytes, 60U, 1U);
  return off::data::AudioBankHeader::parse(bytes);
}
off::graphics::IntroPreparedSound sound(const std::size_t owner = 7U) {
  off::graphics::IntroPreparedSound value{};
  value.directory_index = owner;
  value.source.sound_definition_reference = 9U;
  value.definition = {.definition_offset = 9U, .identifier_offset = 30U,
                      .resource_link = 16U, .duration_bits = 0x3f800000U,
                      .duration = 1.0F, .logical_identifier = "fixture"};
  return value;
}
}

int main() {
  try {
    const auto valid = off::graphics::IntroAudioCueInventory::from_prepared(
        std::array{sound()}, header(), std::array{std::optional<std::size_t>{0U}});
    check(valid.source_owner_count == 1U && valid.distinct_sound_definition_count == 1U &&
              valid.distinct_stream_count == 1U && valid.local_bank_stream_count == 1U &&
              valid.global_bank_stream_count == 0U,
          "complete source-backed relation is inventoried without a cue mapping");
    auto mismatched_definition = sound();
    mismatched_definition.definition.definition_offset = 10U;
    rejects([&] { static_cast<void>(off::graphics::IntroAudioCueInventory::from_prepared(
        std::array{mismatched_definition}, header(), std::array{std::optional<std::size_t>{0U}})); },
        "SND definition identity must match its authored owner reference");
    rejects([&] { static_cast<void>(off::graphics::IntroAudioCueInventory::from_prepared(
        std::array{sound()}, header(), std::array{std::optional<std::size_t>{}})); },
        "every owner must retain its exact prepared WHD row");
    rejects([&] { static_cast<void>(off::graphics::IntroAudioCueInventory::from_prepared(
        std::array{sound(), sound()}, header(),
        std::array{std::optional<std::size_t>{0U}, std::optional<std::size_t>{0U}})); },
        "directory owners cannot be duplicated");
    std::cout << "intro audio cue inventory tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
