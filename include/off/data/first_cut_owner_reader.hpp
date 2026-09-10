#pragma once

#include "off/data/deferred_reader_session.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// Reviewed owner-base envelope for the one supported first-cut list record.
// It exposes a bounded component suffix but does not invoke component readers.
class FirstCutOwnerReader final {
public:
  [[nodiscard]] static DeferredOwnerReaderResult read(
      std::span<const std::byte> owner_block) {
    constexpr std::size_t block_size=171U;
    constexpr std::size_t component_offset=14U;
    constexpr std::size_t component_extent=157U;
    if(owner_block.size()!=block_size || read_u32(owner_block,0U)!=block_size ||
        byte(owner_block,4U)!=0x89U || read_u32(owner_block,5U)!=8U ||
        byte(owner_block,13U)!=0x06U || byte(owner_block,170U)!=0xffU)
      fail();

    return {owner_block.subspan(component_offset),component_extent};
  }

private:
  [[nodiscard]] static std::uint8_t byte(std::span<const std::byte> bytes,
                                         std::size_t offset) {
    if(offset>=bytes.size()) fail();
    return std::to_integer<std::uint8_t>(bytes[offset]);
  }
  [[nodiscard]] static std::uint32_t read_u32(std::span<const std::byte> bytes,
                                               std::size_t offset) {
    if(offset>bytes.size() || 4U>bytes.size()-offset) fail();
    std::uint32_t value{};
    for(std::size_t index=0;index<4U;++index)
      value|=static_cast<std::uint32_t>(byte(bytes,offset+index))<<(8U*index);
    return value;
  }
  [[noreturn]] static void fail() {
    throw std::runtime_error("First-cut owner reader source is unsupported or malformed");
  }
};

} // namespace off::data
