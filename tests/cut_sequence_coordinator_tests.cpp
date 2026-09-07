#include "off/cutscene/cut_sequence_coordinator.hpp"
#include "off/cutscene/active_cut_update.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using off::cutscene::CutSequenceAdvanceResult;
using off::cutscene::CutSequenceCoordinator;
using off::cutscene::CutSequenceEntry;
using off::cutscene::CutSequenceStartServices;
using off::cutscene::ActiveCutMember;
using off::cutscene::ActiveCutUpdate;
using off::cutscene::ActiveCutUpdateResult;
using off::cutscene::ActiveCutUpdateServices;
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
  {
    static_assert(!std::is_copy_constructible_v<ActiveCutUpdate> && !std::is_move_constructible_v<ActiveCutUpdate>);
    ActiveCutUpdate update({{10U, 0.0F, 1.0F}, {20U, 0.0F, 2.0F}}, 3.0F);
    std::uint32_t sampled = 0U;
    int samples = 0;
    std::vector<std::string> trace;
    ActiveCutUpdateServices update_services{
      .sample_scene_clock = [&] { ++samples; return sampled; },
      .resolve_member = [](std::uint64_t target) { return std::optional<std::uint64_t>{target + 100U}; },
      .start_member = [&](std::uint64_t target, std::size_t index) { trace.push_back("start" + std::to_string(index) + ":" + std::to_string(target)); },
      .end_member = [&](std::uint64_t target, std::size_t index) { trace.push_back("end" + std::to_string(index) + ":" + std::to_string(target)); },
      .cleanup_started_member = [&](std::uint64_t target, std::size_t index) { trace.push_back("cleanup-start" + std::to_string(index) + ":" + std::to_string(target)); },
      .cleanup_ended_member = [&](std::uint64_t target, std::size_t index) { trace.push_back("cleanup-end" + std::to_string(index) + ":" + std::to_string(target)); },
      .complete = [&](std::uint64_t caller) { trace.push_back("complete:" + std::to_string(caller)); },
    };
    rejects([&] { static_cast<void>(update.update(update_services)); });
    update.start(0U, 77U);
    check(update.update(update_services) == ActiveCutUpdateResult::updated && samples == 1 && trace.empty());
    sampled = 1U;
    check(update.update(update_services) == ActiveCutUpdateResult::updated && samples == 2 &&
          trace == std::vector<std::string>({"start0:110", "start1:120"}));
    sampled = 42U;
    check(update.update(update_services) == ActiveCutUpdateResult::updated &&
          trace == std::vector<std::string>({"start0:110", "start1:120", "end0:110"}) && !update.pending_end());
    sampled = 124U;
    check(update.update(update_services) == ActiveCutUpdateResult::updated &&
          trace == std::vector<std::string>({"start0:110", "start1:120", "end0:110", "end1:120"}) && update.pending_end());
    check(update.update(update_services) == ActiveCutUpdateResult::completed && !update.active() && !update.caller() &&
          trace == std::vector<std::string>({"start0:110", "start1:120", "end0:110", "end1:120",
                                             "cleanup-end0:110", "cleanup-end1:120", "cleanup-start0:110",
                                             "cleanup-start1:120", "complete:77"}));
  }
  {
    ActiveCutUpdate pending({{10U, 0.0F, 100.0F}}, 1000.0F);
    std::uint32_t sampled = 1U;
    std::vector<std::string> trace;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { return sampled; },
      .resolve_member = [](std::uint64_t target) { return std::optional<std::uint64_t>{target}; },
      .start_member = [&](std::uint64_t, std::size_t) { trace.push_back("start"); pending.request_end(); },
      .end_member = [&](std::uint64_t, std::size_t) { trace.push_back("end"); },
      .cleanup_started_member = [&](std::uint64_t, std::size_t) { trace.push_back("cleanup-start"); },
      .cleanup_ended_member = [&](std::uint64_t, std::size_t) { trace.push_back("cleanup-end"); },
      .complete = [&](std::uint64_t) { trace.push_back("complete"); },
    };
    pending.start(0U);
    check(pending.update(services) == ActiveCutUpdateResult::completed &&
          trace == std::vector<std::string>({"start", "cleanup-start", "complete"}));
  }
  {
    ActiveCutUpdate retry({{10U, 0.0F, 100.0F}}, 1000.0F);
    bool available = false;
    int starts = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [] { return 1U; },
      .resolve_member = [&](std::uint64_t target) -> std::optional<std::uint64_t> {
        return available ? std::optional<std::uint64_t>{target} : std::nullopt;
      },
      .start_member = [&](std::uint64_t, std::size_t) {
        ++starts;
        if (starts == 1) throw std::runtime_error("test callback");
      },
      .end_member = [](std::uint64_t, std::size_t) {},
      .cleanup_started_member = [](std::uint64_t, std::size_t) {},
      .cleanup_ended_member = [](std::uint64_t, std::size_t) {},
      .complete = [](std::uint64_t) {},
    };
    retry.start(0U);
    check(retry.update(services) == ActiveCutUpdateResult::updated && starts == 0);
    available = true;
    rejects([&] { static_cast<void>(retry.update(services)); });
    check(retry.active() && starts == 1);
    check(retry.update(services) == ActiveCutUpdateResult::updated && starts == 2);
  }
  return failures == 0 ? 0 : 1;
}
