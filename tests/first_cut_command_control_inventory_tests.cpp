#include "off/data/first_cut_command_control_inventory.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void check(bool value, const char* message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class Function> void rejects(Function function, const char* message) {
  try { function(); } catch (const std::runtime_error&) { return; }
  check(false, message);
}
}  // namespace

int main() {
  using off::data::GmsIntroCutCommandSource;
  std::array<GmsIntroCutCommandSource, 5> commands{};
  commands[0] = {1U, 7U, 11U, 5U, "private-name-a"};
  commands[1] = {1U, 7U, 11U, 5U, "private-name-b"};
  commands[2] = {2U, 0U, 12U, 0U, {}};
  commands[3] = {3U, 9U, 0U, 5U, {}};
  commands[4] = {4U, 9U, 13U, 6U, "private-name-c"};
  const std::array<std::size_t, 3> admitted{0U, 2U, 4U};
  const auto inventory = off::data::inventory_first_cut_command_controls(commands, admitted);
  check(inventory.command_records == 5U && inventory.distinct_raw_control_records == 4U &&
            inventory.nonzero_event_references == 4U && inventory.distinct_event_references == 3U &&
            inventory.nonzero_target_references == 4U && inventory.distinct_target_references == 4U &&
            inventory.nonzero_arguments == 4U && inventory.distinct_arguments == 3U &&
            inventory.nonempty_target_names == 3U &&
            inventory.reader_admitted_picture_commands == 3U &&
            inventory.non_picture_commands == 2U,
        "inventory retains only raw-field cardinalities and picture-index relation");
  auto changed_names = commands;
  changed_names[0].target_name = "different-private-name";
  check(off::data::inventory_first_cut_command_controls(changed_names, admitted) == inventory,
        "target text cannot affect source-free control inventory");
  auto changed_controls = commands;
  changed_controls[0].event_argument = 99U;
  check(off::data::inventory_first_cut_command_controls(changed_controls, admitted).raw_control_digest !=
            inventory.raw_control_digest,
        "opaque digest changes with a raw control word");
  const std::array<std::size_t, 2> duplicate{0U, 0U};
  const std::array<std::size_t, 2> unordered{2U, 1U};
  const std::array<std::size_t, 1> out_of_range{5U};
  rejects([&] { (void)off::data::inventory_first_cut_command_controls(commands, duplicate); },
          "duplicate picture index rejects");
  rejects([&] { (void)off::data::inventory_first_cut_command_controls(commands, unordered); },
          "unordered picture index rejects");
  rejects([&] { (void)off::data::inventory_first_cut_command_controls(commands, out_of_range); },
          "out-of-range picture index rejects");
}
