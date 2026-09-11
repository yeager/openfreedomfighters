#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace off::data {

// The one observed MatPosAnim attachment payload.  This is deliberately a
// fixed schema, not a general compact-value reader.  Raw integer and float
// bit patterns remain available to the owner-side state.
struct MatPosDeferredComponentValues final {
  bool a_secondary{}, a_primary{};
  float scalar{};
  std::array<float, 3> vector{};
  std::array<bool, 4> controls_a{};
  std::uint16_t word{};
  std::array<bool, 2> controls_b{};
  std::int32_t raw_r0{}, raw_r1{};
  float k{};
  std::optional<bool> control_i;
};

class MatPosDeferredComponentReader final {
public:
  [[nodiscard]] static MatPosDeferredComponentValues read(
      std::span<const std::byte> payload) {
    // 16 fixed values occupy 92 bytes; the 103-byte owner form adds i3.
    if (payload.size() != 92U && payload.size() != 97U) fail();
    Cursor c{payload};
    MatPosDeferredComponentValues out;
    out.a_secondary = c.i3(false) != 0;
    out.a_primary = c.i3(false) != 0;
    out.scalar = c.f2(false);
    out.vector[0] = c.d1(false);
    out.vector[1] = c.d1(true);
    out.vector[2] = c.d1(true);
    for (auto& value : out.controls_a) value = c.i3(false) != 0;
    out.word = static_cast<std::uint16_t>(c.i10(false));
    out.controls_b[0] = c.i3(false) != 0;
    out.controls_b[1] = c.i3(false) != 0;
    out.raw_r0 = c.i3(false);
    out.raw_r1 = c.i3(false);
    out.k = c.f2(false);
    if (payload.size() == 97U) out.control_i = c.i3(false) != 0;
    if (!c.empty()) fail();
    return out;
  }

private:
  class Cursor final {
  public:
    explicit Cursor(std::span<const std::byte> input) : input_(input) {}
    [[nodiscard]] bool empty() const noexcept { return at_ == input_.size(); }
    [[nodiscard]] std::int32_t i3(bool continuation) { return std::bit_cast<std::int32_t>(u32(3U, continuation)); }
    [[nodiscard]] std::int32_t i10(bool continuation) { return std::bit_cast<std::int32_t>(u32(10U, continuation)); }
    [[nodiscard]] float f2(bool continuation) { return std::bit_cast<float>(u32(2U, continuation)); }
    [[nodiscard]] float d1(bool continuation) {
      const auto bits = u64(1U, continuation);
      return static_cast<float>(std::bit_cast<double>(bits));
    }
  private:
    [[nodiscard]] std::uint8_t tag(std::uint8_t expected, bool continuation) {
      if (at_ >= input_.size()) fail();
      const auto raw = std::to_integer<std::uint8_t>(input_[at_++]);
      // Bit seven is not part of this component grammar.  It is rejected,
      // rather than silently treated as a generic compact-value modifier.
      if ((raw & 0x80U) != 0U || (raw & 0x3fU) != expected ||
          ((raw & 0x40U) != 0U) != continuation) fail();
      return raw;
    }
    [[nodiscard]] std::uint32_t u32(std::uint8_t expected, bool continuation) {
      static_cast<void>(tag(expected, continuation));
      if (input_.size() - at_ < 4U) fail();
      std::uint32_t value{};
      for (std::size_t i{}; i < 4U; ++i)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(input_[at_ + i])) << (8U * i);
      at_ += 4U;
      return value;
    }
    [[nodiscard]] std::uint64_t u64(std::uint8_t expected, bool continuation) {
      static_cast<void>(tag(expected, continuation));
      if (input_.size() - at_ < 8U) fail();
      std::uint64_t value{};
      for (std::size_t i{}; i < 8U; ++i)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(input_[at_ + i])) << (8U * i);
      at_ += 8U;
      return value;
    }
    std::span<const std::byte> input_;
    std::size_t at_{};
  };
  [[noreturn]] static void fail() { throw std::runtime_error("MatPos deferred component payload is unsupported or malformed"); }
};

} // namespace off::data
