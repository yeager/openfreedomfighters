#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// Read-only contract for the first payload in the reviewed six-attachment
// first-cut owner. It accepts one exact, delimiter-free payload and performs
// no owner, component, event, lifecycle, audio, or renderer operation.
struct FirstCutListComponentRecord final {
  std::array<std::uint32_t,7> controls{};
  float final_value{};
};

class FirstCutListComponentReader final {
public:
  [[nodiscard]] static FirstCutListComponentRecord read(
      std::span<const std::byte> payload) {
    CompactTypedValueDecoder decoder(payload);
    FirstCutListComponentRecord result;
    constexpr std::array<std::uint8_t,7> tags{0x83U,0x83U,0x03U,0x08U,0x03U,0x83U,0x03U};
    for(std::size_t index=0;index<tags.size();++index)
      result.controls[index]=next(decoder,tags[index]);
    result.final_value=std::bit_cast<float>(next(decoder,0x02U));
    if(!std::isfinite(result.final_value) || !decoder.empty()) fail();
    return result;
  }

private:
  [[nodiscard]] static std::uint32_t next(CompactTypedValueDecoder& decoder,
                                          std::uint8_t tag) {
    const auto value=decoder.next();
    if(value.raw_tag!=tag || value.payload.size()!=sizeof(std::uint32_t)) fail();
    return value.u32_bits();
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("First-cut list component payload is unsupported or malformed");
  }
};

} // namespace off::data
