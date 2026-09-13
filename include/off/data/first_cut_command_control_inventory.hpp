#pragma once

#include "off/data/gms_image.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace off::data {

// A deliberately source-free summary of the fixed first-cut command payload.
// It classifies raw, parsed fields only.  In particular, it does not assign a
// meaning to a word, target, event, name, or command-array order.  The two
// digests are deterministic change detectors, not content identifiers and not
// evidence of playback behavior.
struct FirstCutCommandControlInventory final {
  std::size_t command_records{};
  std::size_t distinct_raw_control_records{};
  std::size_t nonzero_event_references{};
  std::size_t distinct_event_references{};
  std::size_t nonzero_target_references{};
  std::size_t distinct_target_references{};
  std::size_t nonzero_arguments{};
  std::size_t distinct_arguments{};
  std::size_t nonempty_target_names{};
  std::size_t reader_admitted_picture_commands{};
  std::size_t non_picture_commands{};
  std::uint64_t raw_control_digest{14695981039346656037ULL};
  std::uint64_t reader_admitted_picture_control_digest{14695981039346656037ULL};

  [[nodiscard]] bool operator==(const FirstCutCommandControlInventory&) const = default;
};

namespace detail {
inline void append_u32(std::uint64_t& digest, std::uint32_t value) noexcept {
  for (unsigned byte = 0; byte < sizeof(value); ++byte) {
    digest ^= static_cast<std::uint8_t>(value >> (byte * 8U));
    digest *= 1099511628211ULL;
  }
}

[[nodiscard]] inline std::array<std::uint32_t, 4> raw_controls(
    const GmsIntroCutCommandSource& command) noexcept {
  return {command.timeline_position, command.event_reference,
          command.target_reference, command.event_argument};
}

inline void append_controls(std::uint64_t& digest,
                            const GmsIntroCutCommandSource& command) noexcept {
  for (const auto word : raw_controls(command)) append_u32(digest, word);
}
}  // namespace detail

// `reader_admitted_picture_indices` is supplied by the separate, completed
// reader/component picture boundary.  It is an array-index relation only; it
// must be strictly increasing, unique, and in bounds.  Passing it here does
// not select a picture or make a command executable.
[[nodiscard]] inline FirstCutCommandControlInventory
inventory_first_cut_command_controls(
    std::span<const GmsIntroCutCommandSource> commands,
    std::span<const std::size_t> reader_admitted_picture_indices) {
  if (commands.empty())
    throw std::runtime_error("first-cut command control inventory has no commands");

  FirstCutCommandControlInventory result;
  result.command_records = commands.size();
  std::vector<std::array<std::uint32_t, 4>> distinct_records;
  std::vector<std::uint32_t> events, targets, arguments;
  distinct_records.reserve(commands.size());
  events.reserve(commands.size());
  targets.reserve(commands.size());
  arguments.reserve(commands.size());
  for (const auto& command : commands) {
    const auto controls = detail::raw_controls(command);
    if (std::find(distinct_records.begin(), distinct_records.end(), controls) ==
        distinct_records.end())
      distinct_records.push_back(controls);
    if (command.event_reference != 0U) ++result.nonzero_event_references;
    if (command.target_reference != 0U) ++result.nonzero_target_references;
    if (command.event_argument != 0U) ++result.nonzero_arguments;
    if (!command.target_name.empty()) ++result.nonempty_target_names;
    if (std::find(events.begin(), events.end(), command.event_reference) == events.end())
      events.push_back(command.event_reference);
    if (std::find(targets.begin(), targets.end(), command.target_reference) == targets.end())
      targets.push_back(command.target_reference);
    if (std::find(arguments.begin(), arguments.end(), command.event_argument) == arguments.end())
      arguments.push_back(command.event_argument);
    detail::append_controls(result.raw_control_digest, command);
  }
  result.distinct_raw_control_records = distinct_records.size();
  result.distinct_event_references = events.size();
  result.distinct_target_references = targets.size();
  result.distinct_arguments = arguments.size();

  std::size_t previous{};
  bool have_previous{};
  for (const auto index : reader_admitted_picture_indices) {
    if (index >= commands.size() || (have_previous && index <= previous))
      throw std::runtime_error("first-cut picture command indices are not canonical");
    detail::append_controls(result.reader_admitted_picture_control_digest,
                            commands[index]);
    previous = index;
    have_previous = true;
    ++result.reader_admitted_picture_commands;
  }
  result.non_picture_commands = commands.size() - result.reader_admitted_picture_commands;
  return result;
}

}  // namespace off::data
