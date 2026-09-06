#include "off/graphics/picture_ordered_draw_loop.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
constexpr std::uint32_t key(std::uint32_t view, std::uint32_t resource, std::uint32_t subtype = 2,
                            std::uint32_t control = 0) {
  return (view << 27U) | (control << 19U) | ((resource & 0x7ffU) << 4U) | subtype;
}
PictureOrderedDrawEntry entry(std::uint32_t k, std::uint64_t record,
                              std::optional<std::uint64_t> view = 55,
                              std::optional<std::uint64_t> resource = 66) {
  return {k, record, view, resource};
}
struct Recorder {
  std::vector<std::string> events;
  std::size_t throw_at{};
  void event(std::string value) {
    events.push_back(std::move(value));
    if (throw_at && events.size() == throw_at) throw std::runtime_error("injected ordered backend failure");
  }
  PictureOrderedDrawHooks hooks() {
    return {[this] { event("reset"); },
            [this](auto id) { event("view:" + std::to_string(id)); },
            [this](auto type) { event("begin:" + std::to_string(type)); },
            [this](auto type) { event("end:" + std::to_string(type)); },
            [this](auto id) { event("bind:" + std::to_string(id)); },
            [this](auto id, auto type) { event("emit:" + std::to_string(id) + ":" + std::to_string(type)); }};
  }
};
}

int main() {
  static_assert(!std::is_copy_constructible_v<PictureOrderedDrawLoop>);
  static_assert(!std::is_move_constructible_v<PictureOrderedDrawLoop>);
  {
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    check(!loop.run({}, cursor, output.hooks()) && cursor == 0 && output.events == std::vector<std::string>{"reset"},
          "empty invocation still resets backend but never invents subtype or cursor movement");
    const std::array records{entry(key(1, 1), 11)};
    cursor = records.size(); output.events.clear();
    check(!loop.run(records, cursor, output.hooks()) && cursor == records.size() && output.events == std::vector<std::string>{"reset"},
          "already exhausted later round resets again");
  }
  const std::array transitions{entry(key(1, 1, 2), 11, 501, 101), entry(key(2, 2, 3), 12, 502, 102)};
  const std::vector<std::string> transition_events{
      "reset", "view:501", "begin:2", "bind:101", "emit:11:2",
      "view:502", "end:2", "begin:3", "bind:102", "emit:12:3", "end:3"};
  {
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    auto hooks = output.hooks(); const auto end = hooks.subtype_end;
    hooks.subtype_end = [&](std::uint8_t type) {
      check(cursor == (type == 3 ? 2U : 0U), "only final subtype end observes published cursor");
      end(type);
    };
    check(!loop.run(transitions, cursor, hooks) && cursor == 2 && output.events == transition_events,
          "view transition precedes subtype end/begin, then binding and emission");
  }
  {
    // Full equality bypasses transition work despite different record metadata.
    const std::array records{entry(key(1, 1), 11, 501, 101), entry(key(1, 1), 12, 999, 888),
                            entry(key(17, 2), 13, 777, 103)};
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    check(!loop.run(records, cursor, output.hooks()), "equal-key/colliding-view sequence exhausts");
    check(output.events == std::vector<std::string>{"reset", "view:501", "begin:2", "bind:101",
          "emit:11:2", "emit:12:2", "bind:103", "emit:13:2", "end:2"},
          "equal keys and equal masked view fields never add hidden pointer-identity transitions");
  }
  {
    const std::array records{entry(key(1, 0x7ff), 11, 501, 101),
                            entry(key(1, 3), 12, 501, std::nullopt),
                            entry(key(1, 3, 2, 1), 13, 501, 103)};
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    (void)loop.run(records, cursor, output.hooks());
    check(output.events == std::vector<std::string>{"reset", "view:501", "begin:2",
          "emit:11:2", "emit:12:2", "emit:13:2", "end:2"},
          "initial binding sentinel and null resource preserve binding while complete previous key still advances");
  }
  {
    const std::array records{entry(key(1, 1), 11, 501, 101),
                            entry(key(15, 9, 3), 12, std::nullopt, 999),
                            entry(key(1, 1), 13, 999, 888)};
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    check(!loop.run(records, cursor, output.hooks()) && cursor == 3, "reserved-view skip continues rather than ending round");
    check(output.events == std::vector<std::string>{"reset", "view:501", "begin:2", "bind:101",
          "emit:11:2", "emit:13:2", "end:2"},
          "changed reserved view skips without modifying previous key/subtype");
  }
  {
    const std::array records{entry(key(15, 1), 11, std::nullopt, 101),
                            entry(key(15, 1), 12, std::nullopt, 999)};
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    check(!loop.run(records, cursor, output.hooks()), "initial reserved-mask sequence exhausts");
    check(output.events == std::vector<std::string>{"reset", "begin:2", "bind:101", "emit:11:2", "emit:12:2", "end:2"},
          "initial reserved field equals sentinel field and emits without a view hook; equality test remains first");
  }
  {
    const std::array records{entry(key(9, 9, 9), 90, std::nullopt, 999),
                            entry(key(1, 1), 11, 501, 101),
                            entry(key(15, 8, 9), 91, std::nullopt, 998),
                            entry(key(1, 1), 12, 501, 101),
                            entry(key(1, 1, 9), 92, std::nullopt, 997)};
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    check(loop.run(records, cursor, output.hooks()) && cursor == 1 && output.events == std::vector<std::string>{"reset"},
          "first barrier advances without view/resource/subtype/emit work and returns remaining work");
    output.events.clear();
    check(loop.run(records, cursor, output.hooks()) && cursor == 3,
          "middle barrier publishes cursor after barrier and reports remaining entries");
    check(output.events == std::vector<std::string>{"reset", "view:501", "begin:2", "bind:101", "emit:11:2", "end:2"},
          "reserved-field barrier is recognized before any view transition or skip");
    output.events.clear();
    check(!loop.run(records, cursor, output.hooks()) && cursor == 5,
          "last barrier returns false because published cursor equals captured end");
    check(output.events == std::vector<std::string>{"reset", "view:501", "begin:2", "bind:101", "emit:12:2", "end:2"},
          "continuation invocation resets local key/subtype and repeats admitted transitions");
  }
  {
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    const std::array bad_sentinel{entry(UINT32_MAX, 1)};
    rejects([&] { (void)loop.run(bad_sentinel, cursor, output.hooks()); }, "unsupported full initial-sentinel collision rejects");
    check(cursor == 0 && output.events.empty(), "sentinel rejection precedes reset as explicit native policy");
    const std::array missing{entry(key(1, 1), 1, std::nullopt)};
    rejects([&] { (void)loop.run(missing, cursor, output.hooks()); }, "missing required view association rejects");
    const std::array equal_missing{entry(key(1, 1), 1), entry(key(1, 1), 2, std::nullopt)};
    rejects([&] { (void)loop.run(equal_missing, cursor, output.hooks()); },
            "broader native policy validates missing association even in a later equal-key slot");
    const std::array after_barrier{entry(key(1, 1, 9), 1, std::nullopt), entry(key(1, 1), 2, std::nullopt)};
    rejects([&] { (void)loop.run(after_barrier, cursor, output.hooks()); },
            "broader native policy validates all remaining entries beyond first barrier");
    cursor = 3;
    rejects([&] { (void)loop.run(transitions, cursor, output.hooks()); }, "out-of-range prepared cursor rejects");
    check(cursor == 3 && output.events.empty(), "invalid inputs never publish cursor or execute reset");
    cursor = 0; auto incomplete = output.hooks(); incomplete.emit = {};
    rejects([&] { (void)loop.run({}, cursor, incomplete); }, "incomplete hooks reject even exhausted operation");
    const std::array prior_invalid{entry(UINT32_MAX, 1), entry(key(1, 1), 2)};
    cursor = 1;
    check(!loop.run(prior_invalid, cursor, output.hooks()) && cursor == 2,
          "prepared cursor is honored; already consumed entries are not revalidated or emitted");
  }
  for (std::size_t failure = 1; failure <= transition_events.size(); ++failure) {
    PictureOrderedDrawLoop loop; Recorder output; output.throw_at = failure; std::size_t cursor = 0;
    rejects([&] { (void)loop.run(transitions, cursor, output.hooks()); }, "backend callback error propagates");
    check(output.events == std::vector<std::string>(transition_events.begin(), transition_events.begin() + failure),
          "callback failure retains completed prefix with no invented finalization");
    check(cursor == (failure == transition_events.size() ? 2U : 0U),
          "cursor is unpublished on early failure but published before final subtype-end failure");
    // Abort the failed frame; demonstrate that a different empty invocation is
    // not mistaken for reentry. This is not transactional retry of its effects.
    Recorder later; std::size_t empty_cursor = 0;
    check(!loop.run({}, empty_cursor, later.hooks()) && later.events == std::vector<std::string>{"reset"},
          "failed invocation releases reentry guard for a separately prepared operation");
  }
  {
    PictureOrderedDrawLoop loop; Recorder output; std::size_t cursor = 0;
    auto hooks = output.hooks(); const auto reset = hooks.reset;
    hooks.reset = [&] {
      std::size_t nested_cursor = 0;
      rejects([&] { (void)loop.run(transitions, nested_cursor, output.hooks()); }, "reset callback cannot reenter loop");
      check(cursor == 0 && nested_cursor == 0 && output.events.empty(), "reentry rejects before nested reset or cursor effects");
      reset();
    };
    check(!loop.run(transitions, cursor, hooks) && cursor == 2 && output.events == transition_events,
          "caught reentry preserves successful outer operation");
  }
  return failures == 0 ? 0 : 1;
}
