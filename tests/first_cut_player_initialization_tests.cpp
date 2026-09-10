#include "off/cutscene/first_cut_player_initialization.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char *message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
template <class Function> void rejects(Function function, const char *message) {
  try { function(); } catch (const std::runtime_error &) { return; }
  check(false, message);
}
off::cutscene::FirstCutPlayerInitialization make_player() {
  off::data::GmsIntroFirstCutSource list;
  for (std::size_t index = 0; index < list.commands.size(); ++index)
    list.commands[index].timeline_position = static_cast<std::uint32_t>(5U - index);
  list.final_value = 1.0F;
  off::data::GmsIntroCutSequenceSource sequence; sequence.values = {0.0F, 175.0F};
  return off::cutscene::FirstCutPlayerInitialization({42U, {10U, 11U, 12U, 13U, 14U}, list, sequence});
}
} // namespace

int main() {
  auto player = make_player();
  std::vector<std::string> trace;
  player.run_phase_one({
      .invoke_command = [&](auto component, const auto&) { trace.push_back("one:" + std::to_string(component)); },
      .read_retained_source = [&] { trace.push_back("read"); },
      .register_list_events = [&] { trace.push_back("events"); },
      .member_count = [&] { trace.push_back("count"); return 1U; },
      .write_queue_property = [&](auto value) { check(value == 77U, "retain caller queue value"); trace.push_back("queue"); },
      .setup_action_map = [&] { trace.push_back("map"); },
      .queue_property_value = 77U});
  check(player.phase_one_complete() && !player.source_read_marker() && player.events_registered() &&
            player.started() == std::vector<bool>{false} && player.completed() == std::vector<bool>{false} &&
            trace == std::vector<std::string>{"one:14", "one:13", "one:12", "one:11", "one:10", "read", "events", "count", "queue", "map"},
        "phase one visits commands in reverse then initializes the cold list state");
  player.run_phase_two({
      .invoke_command = [&](auto component, const auto&) { trace.push_back("two:" + std::to_string(component)); },
      .read_scene_reference = [&](std::string_view key) -> std::optional<std::uint64_t> { trace.push_back(std::string(key)); return key == "rActiveCameraList" ? std::optional<std::uint64_t>{88U} : std::optional<std::uint64_t>{99U}; },
      .resolve_member = [&](std::size_t index) -> std::optional<std::uint64_t> { check(index == 0U, "resolve sole member"); trace.push_back("member"); return 55U; },
      .request_member_info = [&](std::uint64_t member) -> std::optional<off::cutscene::FirstCutMemberInfo> { check(member == 55U, "direct info target"); trace.push_back("info"); return {{175.0F}}; },
      .member_name = [](std::uint64_t) { return std::string_view{"Cut01"}; },
      .resolve_scene_object = [&](std::uint64_t value) -> std::optional<std::uint64_t> { check(value == 99U, "resolve cut object source"); trace.push_back("object"); return 100U; },
      .register_ordered_command = [&](auto component, const auto& command) { check(component == 42U, "retain concrete list identity"); trace.push_back("command:" + std::to_string(command.timeline_position)); },
      .close_ordered_command_receiver = [&](auto component) { check(component == 42U, "close concrete list identity"); trace.push_back("close"); }});
  check(player.phase_two_complete() && player.derived_end() == 175.0F && player.active_camera_list() == 88U &&
            player.cut_sequence_object() == 100U &&
            trace == std::vector<std::string>{"one:14", "one:13", "one:12", "one:11", "one:10", "read", "events", "count", "queue", "map", "two:14", "command:1", "two:13", "command:2", "two:12", "command:3", "two:11", "command:4", "two:10", "command:5", "rActiveCameraList", "member", "info", "rCutSequenceObject", "object", "close"},
        "phase two registers each eligible command after its reverse callback without playback");
  off::cutscene::FirstCutPlayerListReceiver receiver(42U);
  rejects([&] { receiver.register_ordered_command(42U, {}); }, "receiver is cold before phase one");
  receiver.open_after_phase_one(42U);
  off::data::GmsIntroCutCommandSource late{}, early{}, equal{};
  late.timeline_position = 9U; early.timeline_position = 1U; equal.timeline_position = 1U;
  receiver.register_ordered_command(42U, late);
  receiver.register_ordered_command(42U, early);
  receiver.register_ordered_command(42U, equal);
  check(receiver.commands().size() == 3U && receiver.commands()[0].timeline_position == 1U &&
            receiver.commands()[1].timeline_position == 1U && receiver.commands()[2].timeline_position == 9U,
        "receiver owns cached strict command insertion after phase one");
  off::data::GmsIntroCutCommandSource negative{}; negative.timeline_position = 0x80000000U;
  rejects([&] { receiver.register_ordered_command(42U, negative); }, "receiver rejects signed-negative commands");
  receiver.close_after_phase_two(42U);
  rejects([&] { receiver.register_ordered_command(42U, early); }, "receiver closes after phase two");
  off::data::GmsIntroFirstCutSource session_list;
  session_list.commands[0].timeline_position = 4U;
  session_list.commands[1].timeline_position = 1U;
  session_list.commands[2].timeline_position = 1U;
  session_list.commands[3].timeline_position = 0x80000000U;
  session_list.commands[4].timeline_position = 0x80000000U;
  session_list.final_value = 1.0F;
  off::data::GmsIntroCutSequenceSource session_sequence; session_sequence.values = {0.0F, 1.0F};
  off::cutscene::FirstCutPlayerSession session({42U, {10U, 11U, 12U, 13U, 14U}, session_list, session_sequence});
  session.run_phase_one({.invoke_command=[](auto,const auto&) {}, .read_retained_source=[] {},
                         .register_list_events=[] {}, .member_count=[] { return 0U; },
                         .write_queue_property=[](auto) {}});
  check(session.receiver().open() && !session.receiver().closed(), "session opens its private receiver after phase one");
  session.run_phase_two({
      .invoke_command=[](auto,const auto&) {},
      .read_scene_reference=[](std::string_view) -> std::optional<std::uint64_t> { return std::nullopt; },
      .resolve_member=[](std::size_t) -> std::optional<std::uint64_t> { return std::nullopt; },
      .request_member_info=[](std::uint64_t) -> std::optional<off::cutscene::FirstCutMemberInfo> { return std::nullopt; },
      .member_name=[](std::uint64_t) { return std::string_view{}; },
      .resolve_scene_object=[](std::uint64_t) -> std::optional<std::uint64_t> { return std::nullopt; }});
  check(session.initialization().phase_two_complete() && session.receiver().closed() &&
            session.receiver().commands().size() == 3U &&
            session.receiver().commands()[0].timeline_position == 1U &&
            session.receiver().commands()[1].timeline_position == 1U &&
            session.receiver().commands()[2].timeline_position == 4U,
        "session alone binds first-cut command admission and never starts playback");
  auto failing = make_player();
  rejects([&] { failing.run_phase_one({
      .invoke_command = [](auto, const auto&) { throw std::runtime_error("injected"); },
      .read_retained_source = [] {}, .register_list_events = [] {}, .member_count = [] { return 1U; },
      .write_queue_property = [](auto) {}}); }, "reject a failed phase-one callback");
  rejects([&] { failing.run_phase_one({
      .invoke_command = [](auto, const auto&) {}, .read_retained_source = [] {},
      .register_list_events = [] {}, .member_count = [] { return 1U; }, .write_queue_property = [](auto) {}}); },
      "failed initialization does not permit a retry");
  std::cout << "first-cut player initialization tests passed\n";
}
