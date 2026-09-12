#pragma once

#include <array>
#include <cstdint>

namespace off::data {

// The owner lookup uses an opaque native selector. Preserve its byte order
// independently of host endianness; it is not a display name or general
// property-key encoding.
inline constexpr std::array<char, 4> matpos_owner_child_selector{'S', 'Y', 'E', 'K'};
inline constexpr std::uint32_t matpos_owner_child_selector_word = 0x4b455953U;

} // namespace off::data
