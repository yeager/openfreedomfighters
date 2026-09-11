#pragma once

#include "off/data/deferred_reader_session.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// The one observed, header-inclusive VertAnim component record. This is not a
// general compact-value reader and does not run animation or lifecycle work.
struct VertAnimDeferredComponentValues final {
  bool mirrored_first_control{};
  bool primary_control{};
  float rate{};
  bool control_a{}, control_b{};
  std::uint16_t short_control{};
  bool control_c{}, control_d{}, control_e{};
  std::int32_t raw_word_a{}, raw_word_b{};
  DeferredOwnerReaderResult component_tail;
};

class VertAnimDeferredComponentReader final {
public:
  [[nodiscard]] static VertAnimDeferredComponentValues read(
      std::span<const std::byte> record) {
    constexpr std::size_t record_size = 61U;
    constexpr std::array<std::uint8_t, 11U> classes{3U, 3U, 2U, 3U, 3U,
                                                      10U, 3U, 3U, 3U, 3U, 3U};
    if (record.size() != record_size || u32(record, 0U) != record_size ||
        byte(record, 59U) != 0x06U || byte(record, 60U) != 0xffU)
      fail();
    // Exactly the three owned-data tag forms are supported: no high bits,
    // high bits on fields 1/2, or high bits on fields 1/2/4. Bit six never
    // occurs in this form.
    const auto high = [](std::size_t field) {
      return field == 0U || field == 1U || field == 3U;
    };
    bool first_two_high{};
    bool fourth_high{};
    for (std::size_t field{}; field < classes.size(); ++field) {
      const auto raw = byte(record, 4U + field * 5U);
      if ((raw & 0x40U) != 0U || (raw & 0x3fU) != classes[field]) fail();
      const bool set = (raw & 0x80U) != 0U;
      if (field == 0U || field == 1U) first_two_high |= set;
      else if (field == 3U) fourth_high = set;
      else if (set) fail();
      if (set && !high(field)) fail();
    }
    if (((byte(record, 4U) & 0x80U) != (byte(record, 9U) & 0x80U)) ||
        (fourth_high && !first_two_high)) fail();
    const auto i = [&](std::size_t field) { return std::bit_cast<std::int32_t>(u32(record, 5U + field * 5U)); };
    return {.mirrored_first_control = i(0U) != 0,
            .primary_control = i(1U) != 0,
            .rate = std::bit_cast<float>(u32(record, 15U)),
            .control_a = i(3U) != 0,
            .control_b = i(4U) != 0,
            .short_control = static_cast<std::uint16_t>(i(5U)),
            .control_c = i(6U) != 0,
            .control_d = i(7U) != 0,
            .control_e = i(8U) != 0,
            .raw_word_a = i(9U),
            .raw_word_b = i(10U),
            .component_tail = {record.subspan(60U), 1U}};
  }

private:
  [[nodiscard]] static std::uint8_t byte(std::span<const std::byte> bytes,
                                          std::size_t at) {
    if (at >= bytes.size()) fail();
    return std::to_integer<std::uint8_t>(bytes[at]);
  }
  [[nodiscard]] static std::uint32_t u32(std::span<const std::byte> bytes,
                                          std::size_t at) {
    if (at > bytes.size() || 4U > bytes.size() - at) fail();
    std::uint32_t out{};
    for (std::size_t index{}; index < 4U; ++index)
      out |= static_cast<std::uint32_t>(byte(bytes, at + index)) << (8U * index);
    return out;
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("VertAnim deferred component record is unsupported or malformed");
  }
};

} // namespace off::data
