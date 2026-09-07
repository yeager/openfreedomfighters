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
using off::cutscene::ActiveCutTrackingCollection;
using off::cutscene::ActiveCutTrackingRegistration;
using off::cutscene::ActiveCutPreludeInput;
using off::cutscene::ActiveCutPreludeServices;
using off::cutscene::ActiveCutTailInput;
using off::cutscene::ActiveCutTailInputState;
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
    ActiveCutUpdate update({{10U, 0.0F, 1.0F}, {10U, 0.0F, 2.0F}}, 3.0F);
    std::uint32_t sampled = 1U;
    int samples = 0;
    std::vector<std::string> trace;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { ++samples; return sampled; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{{110U, 10U, ActiveCutTrackingCollection::primary}}; },
      .start_member = [&](std::uint64_t target, std::size_t index) { trace.push_back("start" + std::to_string(index) + ":" + std::to_string(target)); },
      .end_primary_member = [&](std::uint64_t target, std::uint64_t source, std::size_t index, bool enabled) { trace.push_back("primary" + std::to_string(index) + ":" + std::to_string(target) + ":" + std::to_string(source) + ":" + std::to_string(enabled)); },
      .resolve_retained_source = [](std::uint64_t source) { return std::optional<std::uint64_t>{source + 1000U}; },
      .end_secondary_member = [&](std::uint64_t, std::uint64_t, std::size_t, bool) { trace.push_back("secondary"); },
      .complete = [&](std::uint64_t caller) { trace.push_back("complete:" + std::to_string(caller)); },
    };
    rejects([&] { static_cast<void>(update.update(services)); });
    update.start(0U, 77U);
    check(update.update(services) == ActiveCutUpdateResult::updated && samples == 1 &&
          trace == std::vector<std::string>({"start0:110", "start1:110"}));
    sampled = 42U;
    check(update.update(services) == ActiveCutUpdateResult::updated && trace == std::vector<std::string>({"start0:110", "start1:110"}));
    sampled = 124U;
    check(update.update(services) == ActiveCutUpdateResult::updated && update.pending_end() &&
          trace == std::vector<std::string>({"start0:110", "start1:110", "primary0:110:10:1"}));
    check(update.update(services) == ActiveCutUpdateResult::completed && !update.active() && !update.caller() &&
          trace == std::vector<std::string>({"start0:110", "start1:110", "primary0:110:10:1", "complete:77"}));
  }
  {
    // The optional command phase observes the one sampled timeline position
    // after both member passes. It still runs when a member requests end, and
    // therefore precedes synchronous cleanup and completion.
    ActiveCutUpdate update({{10U, 0.0F, 0.5F}}, 100.0F);
    int samples = 0;
    std::vector<std::string> trace;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { ++samples; return 42U; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{{110U, 10U, ActiveCutTrackingCollection::primary}}; },
      .start_member = [&](std::uint64_t, std::size_t) { trace.push_back("start"); },
      .end_primary_member = [&](std::uint64_t, std::uint64_t, std::size_t, bool) { trace.push_back("end"); },
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) { check(false); },
      .run_due_commands = [&](float position) {
        check(position == 1.025390625F);
        trace.push_back("commands");
      },
      .complete = [&](std::uint64_t) { trace.push_back("complete"); },
    };
    update.start(0U);
    check(update.update(services) == ActiveCutUpdateResult::updated && samples == 1 &&
          trace == std::vector<std::string>({"start", "end", "commands"}));
    update.request_end();
    check(update.update(services) == ActiveCutUpdateResult::completed && samples == 2 &&
          trace == std::vector<std::string>({"start", "end", "commands", "commands", "complete"}));
  }
  {
    // An unset optional command phase neither changes service validation nor
    // inserts an observable callback between member passes and completion.
    ActiveCutUpdate update({}, 100.0F);
    int completions = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [] { return 1U; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) { check(false); },
      .complete = [&](std::uint64_t) { ++completions; },
    };
    update.start(0U);
    update.request_end();
    check(update.update(services) == ActiveCutUpdateResult::completed && completions == 1);
  }
  {
    // A primary miss alone reaches the identity-matched secondary fallback.
    ActiveCutUpdate update({{200U, 0.0F, 1.0F}, {200U, 0.0F, 2.0F, false}}, 100.0F);
    std::uint32_t sampled = 1U;
    std::vector<std::string> trace;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { return sampled; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{{200U, 900U, ActiveCutTrackingCollection::secondary}}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .resolve_retained_source = [](std::uint64_t source) { return std::optional<std::uint64_t>{source + 1U}; },
      .end_secondary_member = [&](std::uint64_t resource, std::uint64_t target, std::size_t index, bool enabled) { trace.push_back(std::to_string(resource) + ":" + std::to_string(target) + ":" + std::to_string(index) + ":" + std::to_string(enabled)); },
      .complete = [](std::uint64_t) {},
    };
    update.start(0U);
    check(update.update(services) == ActiveCutUpdateResult::updated);
    sampled = 42U;
    check(update.update(services) == ActiveCutUpdateResult::updated && trace.empty());
    sampled = 124U;
    check(update.update(services) == ActiveCutUpdateResult::updated &&
          trace == std::vector<std::string>({"901:200:0:0"}));
  }
  {
    // A false member-info Boolean suppresses only the primary callback. The
    // zero-count entry is still removed, so final cleanup cannot replay it.
    ActiveCutUpdate update({{10U, 0.0F, 1.0F, false}}, 100.0F);
    std::uint32_t sampled = 1U;
    int primary_ends = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { return sampled; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{{110U, 10U, ActiveCutTrackingCollection::primary}}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [&](std::uint64_t, std::uint64_t, std::size_t, bool) { ++primary_ends; },
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) { check(false); },
      .complete = [](std::uint64_t) {},
    };
    update.start(0U);
    check(update.update(services) == ActiveCutUpdateResult::updated);
    sampled = 42U;
    check(update.update(services) == ActiveCutUpdateResult::updated && primary_ends == 0);
    update.request_end();
    check(update.update(services) == ActiveCutUpdateResult::completed && primary_ends == 0);
  }
  {
    // Start consults both collections before accepting a resolver's fresh
    // collection choice, retaining the original selected identity on a hit.
    ActiveCutUpdate update({{10U, 0.0F, 100.0F}, {20U, 0.0F, 100.0F}}, 1000.0F);
    std::vector<std::uint64_t> starts;
    int primary_ends = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [] { return 1U; },
      .resolve_member = [](std::uint64_t target) { return std::optional<ActiveCutTrackingRegistration>{target == 10U ? ActiveCutTrackingRegistration{110U, 77U, ActiveCutTrackingCollection::primary} : ActiveCutTrackingRegistration{220U, 77U, ActiveCutTrackingCollection::secondary}}; },
      .start_member = [&](std::uint64_t target, std::size_t index) { starts.push_back(target); if (index == 1U) update.request_end(); },
      .end_primary_member = [&](std::uint64_t, std::uint64_t, std::size_t, bool) { ++primary_ends; },
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) { check(false); },
      .complete = [](std::uint64_t) {},
    };
    update.start(0U);
    check(update.update(services) == ActiveCutUpdateResult::completed &&
          starts == std::vector<std::uint64_t>({110U, 110U}) && primary_ends == 1);
  }
  {
    // Final cleanup does not decrement and sends an unconditional primary end,
    // then a resolved secondary end with true.
    ActiveCutUpdate pending({{10U, 0.0F, 100.0F}, {20U, 0.0F, 100.0F}}, 1000.0F);
    std::vector<std::string> trace;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [] { return 1U; },
      .resolve_member = [](std::uint64_t target) { return std::optional<ActiveCutTrackingRegistration>{target == 10U ? ActiveCutTrackingRegistration{110U, 10U, ActiveCutTrackingCollection::primary} : ActiveCutTrackingRegistration{220U, 20U, ActiveCutTrackingCollection::secondary}}; },
      .start_member = [&](std::uint64_t, std::size_t index) { if (index == 1U) pending.request_end(); },
      .end_primary_member = [&](std::uint64_t target, std::uint64_t, std::size_t, bool enabled) { trace.push_back("primary:" + std::to_string(target) + ":" + std::to_string(enabled)); },
      .resolve_retained_source = [](std::uint64_t source) { return std::optional<std::uint64_t>{source + 1000U}; },
      .end_secondary_member = [&](std::uint64_t resource, std::uint64_t target, std::size_t, bool enabled) { trace.push_back("secondary:" + std::to_string(resource) + ":" + std::to_string(target) + ":" + std::to_string(enabled)); },
      .complete = [&](std::uint64_t) { trace.push_back("complete"); },
    };
    pending.start(0U);
    check(pending.update(services) == ActiveCutUpdateResult::completed &&
          trace == std::vector<std::string>({"primary:110:1", "secondary:1020:220:1", "complete"}));
  }
  {
    // A failed secondary resolution still consumes its zero-count record.
    ActiveCutUpdate update({{200U, 0.0F, 1.0F}}, 100.0F);
    std::uint32_t sampled = 1U;
    int resolves = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { return sampled; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{{200U, 900U, ActiveCutTrackingCollection::secondary}}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .resolve_retained_source = [&](std::uint64_t) -> std::optional<std::uint64_t> { ++resolves; return std::nullopt; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) { check(false); },
      .complete = [](std::uint64_t) {},
    };
    update.start(0U);
    check(update.update(services) == ActiveCutUpdateResult::updated);
    sampled = 42U;
    check(update.update(services) == ActiveCutUpdateResult::updated && resolves == 1);
    update.request_end();
    check(update.update(services) == ActiveCutUpdateResult::completed && resolves == 1);
  }
  {
    // Prelude callbacks are useful only before admission.  Their owner event
    // precedes the transient toggle, and frame reports inactive without
    // demanding the unrelated active-update services.
    ActiveCutUpdate update({}, 100.0F);
    std::vector<std::string> trace;
    const ActiveCutPreludeServices prelude_services{
      .send_shared_owner_event = [&] { trace.push_back("owner"); },
      .toggle_transient_state = [&] { trace.push_back("toggle"); },
    };
    check(update.frame({true, true}, prelude_services, {}, {}) == ActiveCutUpdateResult::inactive &&
          trace == std::vector<std::string>({"owner", "toggle"}));
    rejects([&] { update.run_prelude({true, false}, {}); });
    check(trace == std::vector<std::string>({"owner", "toggle"}));
  }
  {
    // An owner callback may admit the cut.  The remaining inactive-only
    // prelude operation must not run after that admission.
    ActiveCutUpdate update({}, 100.0F);
    std::vector<std::string> trace;
    ActiveCutPreludeServices prelude_services{
      .send_shared_owner_event = [&] { trace.push_back("owner"); update.start(0U); },
      .toggle_transient_state = [&] { trace.push_back("toggle"); },
    };
    ActiveCutUpdateServices services{
      .sample_scene_clock = [] { return 0U; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .complete = [](std::uint64_t) {},
    };
    check(update.frame({true, true}, prelude_services, {}, services) == ActiveCutUpdateResult::updated &&
          update.active() && trace == std::vector<std::string>({"owner"}));
    check(update.frame({true, true}, prelude_services, {}, services) == ActiveCutUpdateResult::updated &&
          trace == std::vector<std::string>({"owner"}));
  }
  {
    // Bypass applies to the ordered release/press input predicates only.
    // Both that route and natural end request cleanup for a later update.
    ActiveCutUpdate update({}, 100.0F);
    std::uint32_t sampled = 1U;
    int completions = 0;
    ActiveCutUpdateServices services{
      .sample_scene_clock = [&] { return sampled; },
      .resolve_member = [](std::uint64_t) { return std::optional<ActiveCutTrackingRegistration>{}; },
      .start_member = [](std::uint64_t, std::size_t) {},
      .end_primary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .resolve_retained_source = [](std::uint64_t) { return std::optional<std::uint64_t>{}; },
      .end_secondary_member = [](std::uint64_t, std::uint64_t, std::size_t, bool) {},
      .complete = [&](std::uint64_t) { ++completions; },
    };
    update.start(0U);
    check(update.update({true, true, true}, services) == ActiveCutUpdateResult::updated &&
          !update.pending_end() &&
          update.tail_input_state() == ActiveCutTailInputState::awaiting_release);
    check(update.update({false, true, false}, services) == ActiveCutUpdateResult::updated &&
          !update.pending_end() &&
          update.tail_input_state() == ActiveCutTailInputState::awaiting_press);
    check(update.update({false, false, true}, services) == ActiveCutUpdateResult::updated &&
          update.pending_end() && completions == 0);
    check(update.update(services) == ActiveCutUpdateResult::completed && !update.active() &&
          completions == 1 &&
          update.tail_input_state() == ActiveCutTailInputState::awaiting_release);

    update.start(0U);
    sampled = 4097U;
    check(update.update({true, false, false}, services) == ActiveCutUpdateResult::updated &&
          update.pending_end() && completions == 1);
    check(update.update(services) == ActiveCutUpdateResult::completed && completions == 2);
  }
  return failures == 0 ? 0 : 1;
}
