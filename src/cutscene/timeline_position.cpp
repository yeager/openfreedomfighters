#include "off/cutscene/timeline_position.hpp"

#include <bit>
#include <cstdint>

namespace off::cutscene {

float timeline_position(std::uint32_t current, std::uint32_t start) noexcept {
    const auto elapsed = std::bit_cast<std::int32_t>(current - start);
    const auto product = static_cast<std::int64_t>(elapsed) * 25600;
    // C++23 specifies arithmetic right shift for signed integers. This product
    // is exactly divisible by 1024; narrowing the shifted value is observable.
    const auto shifted = product >> 10;
    const auto retained = static_cast<std::uint32_t>(shifted);
    const auto signed_word = std::bit_cast<std::int32_t>(retained);
    const float position = static_cast<float>(signed_word);
    return position * 0x1p-10F;
}

} // namespace off::cutscene
