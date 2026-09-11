#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// The one observed, combined ZWINPIC/LensFlare compact record.  This is a
// deliberately closed grammar: it neither attempts to parse other picture
// records nor gives LensFlareControl/LensFlareLights any meaning.
struct LensFlareDeferredReaderValues final {
  std::uint8_t exponent_control{};
  std::uint32_t base_property{};
  std::uint8_t alpha{};
  std::uint8_t alignment{};
  std::uint8_t extension_control{};
  std::uint32_t picture_asset_reference{};
  std::array<float, 3> scalars{};
  std::array<std::int32_t, 3> raw_words{};
};

class LensFlareDeferredReader final {
public:
  [[nodiscard]] static LensFlareDeferredReaderValues read(
      std::span<const std::byte> record) {
    constexpr std::size_t record_size = 68U;
    if (record.size() != record_size || u32(record, 0U) != record_size ||
        byte(record, 66U) != 0x06U || byte(record, 67U) != 0xffU)
      fail();

    Cursor cursor{record.subspan(4U, 63U)};
    constexpr std::array<bool, 5U> owner_high{false, true, false, true, false};
    std::array<std::uint32_t, 5U> owner{};
    for (std::size_t index{}; index < owner.size(); ++index)
      owner[index] = cursor.i3(owner_high[index]);
    cursor.delimiter();
    const auto asset_reference = cursor.i3(false);
    cursor.delimiter();

    LensFlareDeferredReaderValues out;
    out.exponent_control = checked_exponent(owner[0]);
    out.base_property = owner[1];
    out.alpha = clamp_to_u8(owner[2]);
    out.alignment = checked_alignment(owner[3]);
    out.extension_control = clamp_extension(owner[4]);
    out.picture_asset_reference = asset_reference;
    for (auto& scalar : out.scalars) scalar = cursor.f2(true);

    std::array<bool, 3U> final_high{};
    for (std::size_t index{}; index < out.raw_words.size(); ++index)
      out.raw_words[index] = cursor.i3_with_high(final_high[index]);
    const auto allowed = final_high == std::array<bool, 3U>{false, false, false} ||
                         final_high == std::array<bool, 3U>{true, false, true} ||
                         final_high == std::array<bool, 3U>{true, true, true};
    if (!allowed) fail();
    cursor.delimiter();
    if (!cursor.empty()) fail();
    return out;
  }

private:
  class Cursor final {
  public:
    explicit Cursor(std::span<const std::byte> input) : input_(input) {}
    [[nodiscard]] bool empty() const noexcept { return at_ == input_.size(); }
    [[nodiscard]] std::uint32_t i3(bool high) { return word(3U, high); }
    [[nodiscard]] std::int32_t i3_with_high(bool& high) {
      high = tag(3U);
      return std::bit_cast<std::int32_t>(word_after_tag());
    }
    [[nodiscard]] float f2(bool high) {
      if (tag(2U) != high) fail();
      return std::bit_cast<float>(word_after_tag());
    }
    void delimiter() {
      if (at_ >= input_.size() || std::to_integer<std::uint8_t>(input_[at_++]) != 0x06U)
        fail();
    }
  private:
    [[nodiscard]] std::uint32_t word(std::uint8_t expected, bool high) {
      if (tag(expected) != high) fail();
      return word_after_tag();
    }
    [[nodiscard]] bool tag(std::uint8_t expected) {
      if (at_ >= input_.size()) fail();
      const auto raw = std::to_integer<std::uint8_t>(input_[at_++]);
      if ((raw & 0x40U) != 0U || (raw & 0x3fU) != expected) fail();
      return (raw & 0x80U) != 0U;
    }
    [[nodiscard]] std::uint32_t word_after_tag() {
      if (input_.size() - at_ < 4U) fail();
      std::uint32_t out{};
      for (std::size_t index{}; index < 4U; ++index)
        out |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(input_[at_ + index])) <<
               (8U * index);
      at_ += 4U;
      return out;
    }
    std::span<const std::byte> input_;
    std::size_t at_{};
  };

  [[nodiscard]] static std::uint8_t checked_exponent(std::uint32_t value) {
    if (value >= 8U) fail();
    return static_cast<std::uint8_t>(value);
  }
  [[nodiscard]] static std::uint8_t checked_alignment(std::uint32_t value) {
    if (value > 15U) fail();
    return static_cast<std::uint8_t>(value);
  }
  [[nodiscard]] static std::uint8_t clamp_to_u8(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value > 255U ? 255U : value);
  }
  [[nodiscard]] static std::uint8_t clamp_extension(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value > 16U ? 16U : value);
  }
  [[nodiscard]] static std::uint8_t byte(std::span<const std::byte> input,
                                         std::size_t at) {
    if (at >= input.size()) fail();
    return std::to_integer<std::uint8_t>(input[at]);
  }
  [[nodiscard]] static std::uint32_t u32(std::span<const std::byte> input,
                                         std::size_t at) {
    if (at > input.size() || input.size() - at < 4U) fail();
    std::uint32_t out{};
    for (std::size_t index{}; index < 4U; ++index)
      out |= static_cast<std::uint32_t>(byte(input, at + index)) << (8U * index);
    return out;
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("LensFlare deferred record is unsupported or malformed");
  }
};

} // namespace off::data
