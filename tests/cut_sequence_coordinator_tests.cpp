#include "off/cutscene/cut_sequence_coordinator.hpp"

#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using off::cutscene::CutSequenceAdvanceResult;
using off::cutscene::CutSequenceCoordinator;
using off::cutscene::CutSequenceEntry;
using off::cutscene::CutSequenceStartServices;
int failures = 0;
void check(bool value) { if (!value) { ++failures; std::cerr << "FAIL: cut sequence coordinator\n"; } }
template <class F> void rejects(F operation) { bool rejected = false; try { operation(); } catch (const std::runtime_error&) { rejected = true; } check(rejected); }
}

int main() {
  static_assert(!std::is_copy_constructible_v<CutSequenceCoordinator> && !std::is_move_constructible_v<CutSequenceCoordinator>);
  const std::vector<CutSequenceEntry> source{{11U, {101U, 102U}}, {12U, {201U}}};
  CutSequenceCoordinator coordinator(source);
  std::vector<std::uint64_t> calls;
  CutSequenceStartServices services{.start_cut = [&](std::uint64_t value) { calls.push_back(value); }, .start_parallel_group = [&](std::uint64_t value) { calls.push_back(value); }};
  rejects([&] { static_cast<void>(coordinator.complete_current(services)); });
  rejects([&] { coordinator.start(2U, services); });
  coordinator.start(0U, services);
  check(calls == std::vector<std::uint64_t>({11U, 101U, 102U}) && coordinator.active_index() == 0U);
  rejects([&] { coordinator.start(1U, services); });
  check(coordinator.complete_current(services) == CutSequenceAdvanceResult::advanced && calls == std::vector<std::uint64_t>({11U, 101U, 102U, 12U, 201U}) && coordinator.active_index() == 1U);
  check(coordinator.complete_current(services) == CutSequenceAdvanceResult::sequence_exhausted && !coordinator.active_index());
  CutSequenceCoordinator nested(source);
  std::vector<std::uint64_t> nested_calls;
  CutSequenceStartServices nested_services;
  nested_services.start_cut = [&](std::uint64_t value) { nested_calls.push_back(value); if (value == 11U) check(nested.complete_current(nested_services) == CutSequenceAdvanceResult::advanced); };
  nested_services.start_parallel_group = [&](std::uint64_t value) { nested_calls.push_back(value); };
  nested.start(0U, nested_services);
  check(nested_calls == std::vector<std::uint64_t>({11U, 12U, 201U, 101U, 102U}) && nested.active_index() == 1U);
  const std::vector<CutSequenceEntry> invalid_source{{0U, {}}};
  rejects([&] { CutSequenceCoordinator invalid(invalid_source); });
  const std::vector<CutSequenceEntry> no_group_source{{1U, {}}};
  CutSequenceCoordinator no_groups(no_group_source);
  rejects([&] { no_groups.start(0U, {}); });
  const std::vector<CutSequenceEntry> bad_group_source{{1U, {0U}}};
  CutSequenceCoordinator bad_group(bad_group_source);
  rejects([&] { bad_group.start(0U, services); });
  check(bad_group.active_index() == 0U);
  return failures == 0 ? 0 : 1;
}
