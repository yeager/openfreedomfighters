#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace off::data {

// Read-only contract for one of the five command payloads following the first
// ordered payload. It has no scheduling, event, owner, or renderer behavior.
struct FirstCutCommandComponentRecord final {
  std::uint32_t timeline_position{};
  std::uint32_t event_reference{};
  std::uint32_t target_reference{};
  std::uint32_t event_argument{};
  std::string target_name;
};

class FirstCutCommandComponentReader final {
public:
  [[nodiscard]] static FirstCutCommandComponentRecord read(
      std::span<const std::byte> payload) {
    CompactTypedValueDecoder decoder(payload);
    FirstCutCommandComponentRecord result;
    result.timeline_position=integer(decoder);
    result.event_reference=word(decoder,0x8aU);
    result.target_reference=word(decoder,0x88U);
    result.event_argument=integer(decoder);
    const auto name=decoder.next();
    if(name.raw_tag!=0x04U || name.kind!=CompactTypedValueKind::nul_terminated_string ||
        name.payload.empty() || name.payload.back()!=std::byte{0}) fail();
    result.target_name.assign(reinterpret_cast<const char*>(name.payload.data()),name.payload.size()-1U);
    if(!decoder.empty()) fail();
    return result;
  }

private:
  [[nodiscard]] static std::uint32_t word(CompactTypedValueDecoder& decoder,
                                          std::uint8_t tag) {
    const auto value=decoder.next();
    if(value.raw_tag!=tag || value.payload.size()!=sizeof(std::uint32_t)) fail();
    return value.u32_bits();
  }
  [[nodiscard]] static std::uint32_t integer(CompactTypedValueDecoder& decoder) {
    const auto value=decoder.next();
    if((value.raw_tag!=0x03U && value.raw_tag!=0x83U) ||
        value.payload.size()!=sizeof(std::uint32_t)) fail();
    return value.u32_bits();
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("First-cut command component payload is unsupported or malformed");
  }
};

} // namespace off::data
