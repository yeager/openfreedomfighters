#pragma once

#include "off/data/deferred_reader_session.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// Exact, attachment-free ZGROUP deferred owner record recovered for the
// supported installation. This is deliberately not a base-owner parser or a
// generic compact stream reader.
struct BasicGroupDeferredOwnerValues final {
  float scalar{};
  bool third_is_zero{};
  bool fourth_is_zero{};
  DeferredOwnerReaderResult component_tail;
};

class BasicGroupDeferredReader final {
public:
  [[nodiscard]] static BasicGroupDeferredOwnerValues read(std::span<const std::byte> record) {
    constexpr std::size_t record_size = 31U;
    constexpr std::size_t first_tag = 4U;
    constexpr std::size_t second_tag = 9U;
    constexpr std::size_t third_tag = 14U;
    constexpr std::size_t fourth_tag = 19U;
    constexpr std::size_t fifth_tag = 24U;
    constexpr std::size_t delimiter = 29U;
    constexpr std::size_t terminator = 30U;
    if (record.size() != record_size || read_u32(record, 0U) != record_size ||
        !tag(record, first_tag, 3U, true) || !tag(record, second_tag, 2U, false) ||
        !tag(record, third_tag, 3U, false) || !tag(record, fourth_tag, 3U, false) ||
        !tag(record, fifth_tag, 3U, false) || byte(record, delimiter) != 0x06U ||
        byte(record, terminator) != 0xffU)
      fail();
    // Fields one and five are consumed by the original reader but do not
    // affect the supported live Group state. Preserve that absence of effect.
    const auto scalar = std::bit_cast<float>(read_u32(record, second_tag + 1U));
    return {.scalar = scalar,
            .third_is_zero = read_u32(record, third_tag + 1U) == 0U,
            .fourth_is_zero = read_u32(record, fourth_tag + 1U) == 0U,
            .component_tail = {record.subspan(terminator), 1U}};
  }

private:
  [[nodiscard]] static std::uint8_t byte(std::span<const std::byte> value, std::size_t offset) {
    if (offset >= value.size()) fail();
    return std::to_integer<std::uint8_t>(value[offset]);
  }
  [[nodiscard]] static std::uint32_t read_u32(std::span<const std::byte> value, std::size_t offset) {
    if (offset > value.size() || sizeof(std::uint32_t) > value.size() - offset) fail();
    std::uint32_t result{};
    for (std::size_t index{}; index < sizeof(result); ++index)
      result |= static_cast<std::uint32_t>(byte(value, offset + index)) << (index * 8U);
    return result;
  }
  [[nodiscard]] static bool tag(std::span<const std::byte> record, std::size_t offset,
                                std::uint8_t expected_class, bool allow_high_bit) {
    const auto raw = byte(record, offset);
    return (raw & 0x40U) == 0U && (raw & 0x3fU) == expected_class &&
           (allow_high_bit || (raw & 0x80U) == 0U);
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("Basic Group deferred owner record is unsupported or malformed");
  }
};

} // namespace off::data
