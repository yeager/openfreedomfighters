#pragma once

#include <cstdint>

namespace off::cutscene {

// Converts explicit scene-clock words using the supported cut-player arithmetic.
// This does not establish clock units, select a host clock, or advance a player.
[[nodiscard]] float timeline_position(std::uint32_t current, std::uint32_t start) noexcept;

} // namespace off::cutscene
