#include "off/graphics/picture_ordered_coordinator.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
PictureOrderedCoordinatorHooks quiet() {
  return {[](const auto&, const auto&) {},
    [](const auto& state, std::uint64_t record) {
      for (std::size_t i = 0; i < state.entries.size(); ++i)
        if (state.entries[i].record_identity == record) return i;
      return state.entries.size();
    },
    [](const auto&, std::uint64_t) -> std::optional<std::uint8_t> { return {}; },
    [](auto& state, std::uint32_t) { state.cursor = state.entries.size(); return false; },
    [] { return false; }, []() -> std::optional<std::uint64_t> { return {}; },
    [](std::uint64_t) {}};
}
template<std::size_t N>
std::uint32_t run(PictureOrderedCoordinator& coordinator,
    std::array<PictureOrderedState, N>& states, std::uint32_t limit,
    const PictureOrderedCoordinatorHooks& hooks) {
  std::array<PictureOrderedState*, N> snapshot{};
  for (std::size_t i = 0; i < N; ++i) snapshot[i] = &states[i];
  return coordinator.run(snapshot, limit, hooks);
}
}

int main() {
  // Actual ordinary loops: two barriers force three rounds, including reset
  // calls on a second state that was already exhausted in round zero.
  {
    std::array<PictureOrderedDrawEntry, 5> entries{{
      {0x08000012U, 11, 7, 11}, {9, 12, {}, {}},
      {0x08000022U, 13, 7, 13}, {9, 14, {}, {}},
      {0x08000032U, 15, 7, 15}}};
    std::array<PictureOrderedState, 2> states{{{1, entries, {999}, {}}, {2, {}, {888}, {}}}};
    std::array<PictureOrderedDrawLoop, 2> loops;
    std::vector<std::string> events;
    std::vector<std::uint64_t> emitted;
    unsigned resets = 0;
    bool enabled = false;
    auto hooks = quiet();
    hooks.preselect = [&](const auto& state, const auto&) {
      check(state.selected_records.empty(), "selection list cleared before callback");
      events.push_back("prepare" + std::to_string(state.identity));
    };
    hooks.draw = [&](auto& state, std::uint32_t round) {
      events.push_back("draw" + std::to_string(state.identity) + ":" + std::to_string(round));
      if (state.identity == 2) enabled = true;
      return loops[state.identity - 1].run(state.entries, *state.cursor, {
        [&] { ++resets; }, [](auto) {}, [](auto) {}, [](auto) {}, [](auto) {},
        [&](auto record, auto) { emitted.push_back(record); }});
    };
    hooks.special_enabled = [&] { events.push_back("gate"); return enabled; };
    hooks.special_context = [&]() -> std::optional<std::uint64_t> {
      events.push_back("context"); return 91;
    };
    hooks.first_round_service = [&](auto context) {
      check(context == 91, "live scene context reaches typed special service");
      events.push_back("special");
    };
    PictureOrderedCoordinator coordinator;
    check(run(coordinator, states, 3, hooks) == 3, "barriers produce three rounds");
    check(events == std::vector<std::string>{"prepare1", "prepare2", "draw1:0", "draw2:0",
      "gate", "context", "special", "draw1:1", "draw2:1", "draw1:2", "draw2:2"},
      "all-state rounds follow preparation and live first-round service order");
    check(resets == 6 && emitted == std::vector<std::uint64_t>{11, 13, 15},
      "exhausted states reset each round without duplicate emission");
    check(!states[0].cursor && !states[1].cursor && !coordinator.poisoned(),
      "successful restoration makes all cursors inactive");
  }
  {
    std::array<PictureOrderedDrawEntry, 1> entries{{{0x80000122U, 17, 3, 17}}};
    std::array<PictureOrderedState, 1> states{{{1, entries, {}, {}}}};
    auto hooks = quiet();
    unsigned lookups = 0, orders = 0;
    hooks.preselect = [](const auto&, const auto& select) { select(17); select(17); };
    hooks.slot_of = [&](const auto&, auto) { ++lookups; return std::size_t{0}; };
    hooks.draw = [&](auto&, auto) {
      check(entries[0].key == 0xf8000122U, "selection OR preserves bit31 and lower fields");
      entries[0].key |= 0x400U;
      return false;
    };
    hooks.view_order = [&](const auto& state, auto) -> std::optional<std::uint8_t> {
      check(!state.cursor && entries[0].key == 0x80000522U,
        "restore publishes inactive cursor and clears only view mask before lookup");
      return ++orders == 1 ? std::optional<std::uint8_t>{0xff} : std::nullopt;
    };
    PictureOrderedCoordinator coordinator;
    check(run(coordinator, states, 1, hooks) == 1 && lookups == 4 && orders == 2 &&
      entries[0].key == 0x80000522U && states[0].selected_records.size() == 2,
      "duplicate restoration uses live orders including null view and preserves bit31/current bits");
  }
  // Even an empty snapshot executes one round; scene context is read only when enabled.
  for (unsigned mode = 0; mode != 3; ++mode) {
    auto hooks = quiet();
    unsigned gates = 0, contexts = 0, services = 0;
    hooks.special_enabled = [&] { ++gates; return mode != 0; };
    hooks.special_context = [&]() -> std::optional<std::uint64_t> {
      ++contexts; return mode == 2 ? std::optional<std::uint64_t>{42} : std::nullopt;
    };
    hooks.first_round_service = [&](auto) { ++services; };
    PictureOrderedCoordinator coordinator;
    check(coordinator.run({}, 1, hooks) == 1 && gates == 1 && contexts == (mode != 0) &&
      services == (mode == 2), "empty snapshot honors live special gates before completion");
  }
  {
    PictureOrderedCoordinator coordinator;
    auto hooks = quiet();
    rejects([&] { (void)coordinator.run({}, 0, hooks); }, "zero round bound rejected");
    hooks.draw = {};
    rejects([&] { (void)coordinator.run({}, 1, hooks); }, "missing hook rejected even for empty snapshot");
    PictureOrderedState state{7, {}, {8}, 9};
    std::array<PictureOrderedState*, 2> invalid{&state, nullptr};
    rejects([&] { (void)coordinator.run(invalid, 1, quiet()); }, "null state pointer rejected");
    check(!coordinator.poisoned() && state.cursor == 9 &&
      state.selected_records == std::vector<std::uint64_t>{8}, "preflight leaves state untouched and retryable");
  }
  for (unsigned mode = 0; mode != 3; ++mode) {
    std::array<PictureOrderedDrawEntry, 1> entries{{{2, 17, 1, {}}}};
    std::array<PictureOrderedState, 1> states{{{1, entries, {}, {}}}};
    auto hooks = quiet();
    hooks.preselect = [](const auto&, const auto& select) { select(17); };
    hooks.slot_of = [mode](const auto&, auto) { return mode == 0 ? std::size_t{1} : std::size_t{0}; };
    if (mode == 1) entries[0].record_identity = 18;
    if (mode == 2) hooks.view_order = [](const auto&, auto) -> std::optional<std::uint8_t> {
      throw std::runtime_error("missing owner");
    };
    PictureOrderedCoordinator coordinator;
    rejects([&] { (void)run(coordinator, states, 1, hooks); }, "invalid ownership or restoration lookup rejected");
    check(coordinator.poisoned() && entries[0].key == 2 && !states[0].cursor,
      "failure retains exact mark/restore prefix and poisons coordinator");
    rejects([&] { (void)coordinator.run({}, 1, quiet()); }, "poison prevents retry");
  }
  {
    std::array<PictureOrderedState, 1> states{{{1, {}, {}, {}}}};
    auto hooks = quiet();
    unsigned draws = 0, services = 0;
    hooks.draw = [&](auto&, auto) { ++draws; return true; };
    hooks.special_enabled = [] { return true; };
    hooks.special_context = [] { return std::optional<std::uint64_t>{1}; };
    hooks.first_round_service = [&](auto) { ++services; };
    PictureOrderedCoordinator coordinator;
    rejects([&] { (void)run(coordinator, states, 2, hooks); }, "persistent more work hits explicit bound");
    check(draws == 2 && services == 1 && states[0].cursor == 0 && coordinator.poisoned(),
      "round limit does not synthesize restoration or skip first-round service");
  }
  for (bool overflow : {false, true}) {
    std::array<PictureOrderedDrawEntry, 1> entries{{{2, 17, 1, {}}}};
    std::array<PictureOrderedState, 1> states{{{1, entries, {}, {}}}};
    auto hooks = quiet();
    hooks.preselect = [overflow](const auto&, const auto& select) {
      for (unsigned i = 0; i < 8192U + static_cast<unsigned>(overflow); ++i) select(17);
    };
    PictureOrderedCoordinator coordinator;
    if (overflow) rejects([&] { (void)run(coordinator, states, 1, hooks); }, "8193rd selection rejected before append");
    else check(run(coordinator, states, 1, hooks) == 1, "8192 duplicate selections accepted");
    check(states[0].selected_records.size() == 8192 && coordinator.poisoned() == overflow,
      "selection capacity counts duplicates and never overflows");
  }
  {
    PictureOrderedCoordinator coordinator;
    std::array<PictureOrderedState, 1> states{{{1, {}, {}, {}}}};
    auto hooks = quiet();
    hooks.draw = [&](auto&, auto) -> bool { (void)coordinator.run({}, 1, quiet()); return false; };
    rejects([&] { (void)run(coordinator, states, 1, hooks); }, "callback reentry rejected");
    check(coordinator.poisoned() && states[0].cursor == 0, "propagated reentry preserves active prefix and poisons");
  }
  {
    PictureOrderedCoordinatorHooks::Select escaped;
    std::array<PictureOrderedState, 1> states{{{1, {}, {}, {}}}};
    auto hooks = quiet();
    hooks.preselect = [&](const auto&, const auto& select) { escaped = select; };
    PictureOrderedCoordinator coordinator;
    check(run(coordinator, states, 1, hooks) == 1, "selection callback may copy visitor without invoking later");
    rejects([&] { escaped(17); }, "escaped selection visitor safely rejects after callback lifetime");
    check(states[0].selected_records.empty(), "escaped visitor cannot mutate completed selection");
  }
  {
    PictureOrderedState state{7, {}, {}, {}};
    std::array<PictureOrderedState*, 2> snapshot{&state, &state};
    auto hooks = quiet();
    unsigned preparations = 0, draws = 0;
    hooks.preselect = [&](const auto&, const auto&) { ++preparations; };
    hooks.draw = [&](auto& current, auto) {
      check(&current == &state, "duplicate snapshot slots resolve the same live state");
      ++draws; return false;
    };
    PictureOrderedCoordinator coordinator;
    check(coordinator.run(snapshot, 1, hooks) == 1 && preparations == 2 && draws == 2 && !state.cursor,
      "duplicate snapshot slots are preserved through preparation, drawing and cleanup");
  }
  for (unsigned fail_at = 1; fail_at <= 8; ++fail_at) {
    std::array<PictureOrderedDrawEntry, 1> entries{{{2, 17, 1, {}}}};
    std::array<PictureOrderedState, 1> states{{{1, entries, {}, {}}}};
    auto hooks = quiet();
    unsigned calls = 0;
    const auto event = [&] {
      if (++calls == fail_at) throw std::runtime_error("injected coordinator callback failure");
    };
    hooks.preselect = [&](const auto&, const auto& select) { event(); select(17); };
    hooks.slot_of = [&](const auto&, auto) { event(); return std::size_t{0}; };
    hooks.draw = [&](auto&, auto) { event(); return false; };
    hooks.special_enabled = [&] { event(); return true; };
    hooks.special_context = [&] { event(); return std::optional<std::uint64_t>{1}; };
    hooks.first_round_service = [&](auto) { event(); };
    hooks.view_order = [&](const auto&, auto) { event(); return std::optional<std::uint8_t>{1}; };
    PictureOrderedCoordinator coordinator;
    rejects([&] { (void)run(coordinator, states, 1, hooks); }, "callback failure stops orchestration");
    check(calls == fail_at && coordinator.poisoned(), "no later callbacks run after injected failure");
    check(states[0].cursor.has_value() == (fail_at >= 3 && fail_at <= 6),
      "callback failures preserve actual cursor publication prefix");
    check(entries[0].key == ((fail_at >= 3 && fail_at <= 7) ? 0x78000002U : 2U),
      "callback failures preserve actual marking and restoration mask prefix");
  }
  return failures == 0 ? 0 : 1;
}
