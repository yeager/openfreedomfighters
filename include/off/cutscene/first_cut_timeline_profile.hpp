#pragma once

#include "off/data/gms_image.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::cutscene {

// Structural timing evidence from the closed, ordered first-cut receiver.
// It deliberately retains no event, target, name, or argument data. The
// profile is an admission prerequisite for a future clock-driven player, not
// a scheduler or a substitute cutscene timer.
struct FirstCutTimelineProfile final {
  std::size_t command_count{};
  std::size_t distinct_positions{};
  std::int32_t final_position{};
  std::uint64_t position_digest{14695981039346656037ULL};

  [[nodiscard]] bool operator==(const FirstCutTimelineProfile&) const = default;
};

[[nodiscard]] inline FirstCutTimelineProfile profile_first_cut_timeline(
    std::span<const data::GmsIntroCutCommandSource> commands) {
  if(commands.empty())
    throw std::runtime_error("first-cut timeline has no ordered commands");
  FirstCutTimelineProfile result;
  std::int32_t previous{};
  bool have_previous{};
  for(const auto& command:commands) {
    const auto position=std::bit_cast<std::int32_t>(command.timeline_position);
    if(position<0 || (have_previous && position<previous))
      throw std::runtime_error("first-cut timeline is not ordered by nonnegative position");
    if(!have_previous || position!=previous)
      ++result.distinct_positions;
    for(unsigned byte=0;byte<sizeof(position);++byte) {
      result.position_digest^=static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(position)>>(byte*8U));
      result.position_digest*=1099511628211ULL;
    }
    previous=position;
    have_previous=true;
    ++result.command_count;
  }
  result.final_position=previous;
  return result;
}

} // namespace off::cutscene
