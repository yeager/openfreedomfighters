#include "off/cutscene/command_pass.hpp"
#include "off/cutscene/first_cut_clocked_command_runner.hpp"
#include "off/cutscene/cut_sequence_list.hpp"
#include "off/cutscene/first_cut_command_session.hpp"
#include "off/cutscene/external_cut_sequence_command.hpp"
#include "off/cutscene/external_cut_sequence_list_join.hpp"
#include "off/runtime/intro_live_target_registry.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>

namespace {
using off::cutscene::CommandPass;
using off::cutscene::CommandDeliveryAdapter;
using off::cutscene::CommandDeliveryResult;
using off::cutscene::CommandDeliveryServices;
using off::cutscene::FirstCutCommandSession;
using off::cutscene::FirstCutCommandSessionServices;
using off::cutscene::ExternalCutSequenceCommand;
using off::cutscene::ExternalCutSequenceCommandPhaseTwoServices;
using off::cutscene::ExternalCutSequenceCommandReader;
using off::runtime::IntroLiveTargetRegistry;
using Command = off::data::GmsIntroCutCommandSource;
int failures = 0;
void check(bool value) { if (!value) { ++failures; std::cerr << "FAIL: cut command pass\n"; } }
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected);
}
}
int main() {
    static_assert(!std::is_copy_constructible_v<CommandPass> && !std::is_move_constructible_v<CommandPass>);
    static_assert(!std::is_copy_constructible_v<CommandDeliveryAdapter> &&
                  !std::is_move_constructible_v<CommandDeliveryAdapter>);
    std::array<Command, 4> commands{};
    commands[2].timeline_position = 1;
    commands[0].target_name = "owned";
    CommandPass pass(commands, 20.0F);
    commands[0].target_name = "changed";
    std::vector<std::size_t> visits;
    const CommandPass::Visitor visitor = [&](const Command& command, std::size_t index) {
        if (index == 0U) check(command.target_name == "owned");
        visits.push_back(index);
    };
    pass.run(0, visitor); check(visits.empty());
    pass.run(1, visitor); check(visits == std::vector<std::size_t>({1,3,0}));
    pass.run(2, visitor); check(visits == std::vector<std::size_t>({1,3,0,2}));
    pass.run(3, visitor); check(visits.size() == 4U);
    pass.reset_start(); visits.clear(); pass.run(3, visitor); check(visits.size() == 4U);
    pass.reset_start(); visits.clear();
    rejects([&] { pass.run(3, {}); });
    for (float value : {100020.0F, 100021.0F, std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        rejects([&] { pass.run(value, visitor); });
    check(visits.empty());
    pass.run(-1, visitor); check(visits.empty());
    int attempts = 0;
    rejects([&] { pass.run(3, [&](const Command&, std::size_t index) {
        visits.push_back(index); if (++attempts == 2) throw std::runtime_error("callback");
    }); });
    check(visits == std::vector<std::size_t>({1,3}));
    visits.clear(); pass.run(3, visitor); check(visits == std::vector<std::size_t>({3,0,2}));
    pass.reset_start(); visits.clear();
    pass.run(3, [&](const Command& command, std::size_t index) {
        rejects([&] { pass.reset_start(); }); rejects([&] { pass.run(3, visitor); });
        visitor(command, index);
    });
    check(visits.size() == 4U);
    std::array<Command, 3> late{};
    late[0].timeline_position = 7; late[1].timeline_position = 0xffffffffU; late[2].timeline_position = 7;
    CommandPass late_pass(late, 8); visits.clear();
    late_pass.run(1, visitor); late_pass.run(7, visitor); check(visits.empty());
    late_pass.run(8, [&](const Command&, std::size_t index) { visits.push_back(index); });
    check(visits == std::vector<std::size_t>({2,0}));
    std::array<Command, 2> collision{};
    collision[0].timeline_position = 16777216U; collision[1].timeline_position = 16777217U;
    CommandPass collision_pass(collision, 16777220.0F); visits.clear();
    collision_pass.run(16777216.0F, visitor); check(visits.empty());
    collision_pass.run(16777218.0F, [&](const Command&, std::size_t index) { visits.push_back(index); });
    check(visits == std::vector<std::size_t>({1,0}));
    CommandPass empty({}, 0); empty.run(1, visitor); empty.run(99999, visitor);
    empty.run(std::nextafter(100000.0F, 0.0F), visitor);
    rejects([&] { empty.run(100000, visitor); });
    CommandPass rounded({}, 0.999F);
    rounded.run(100000.0F, visitor);
    rejects([&] { rounded.run(100001.0F, visitor); });
    CommandPass negative_fraction({}, -99999.75F);
    negative_fraction.run(-1.0F, visitor);
    rejects([&] { negative_fraction.run(0.0F, visitor); });
    std::array<Command, 2> negative{};
    negative[0].timeline_position = 0xffffffffU;
    negative[1].timeline_position = 0x80000000U;
    CommandPass all_negative(negative, 0);
    visits.clear();
    all_negative.run(1.0F, visitor);
    check(visits.empty());
    std::array<Command, 1> at_zero{};
    CommandPass tiny(at_zero, 0);
    tiny.run(0.0F, [&](const Command&, std::size_t) { visits.push_back(0); });
    check(visits.empty());
    tiny.run(std::nextafter(0.0F, 1.0F), [&](const Command&, std::size_t) { visits.push_back(0); });
    check(visits == std::vector<std::size_t>{0});
    rejects([&] { CommandPass invalid({}, 2147000000.0F); });
    for (float end : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
                       0x1p31F, -0x1p32F}) rejects([&] { CommandPass invalid({}, end); });

    std::vector<std::string> delivery_trace;
    std::vector<std::uint64_t> dispatched_targets;
    CommandDeliveryServices delivery_services{
        .resolve_event = [&](std::uint32_t reference) -> std::optional<std::uint16_t> {
            delivery_trace.push_back("event:" + std::to_string(reference));
            return reference == 7U ? std::optional<std::uint16_t>{0x401U} : std::nullopt;
        },
        .resolve_reference = [&](std::uint32_t reference) -> std::optional<std::uint64_t> {
            delivery_trace.push_back("reference:" + std::to_string(reference));
            return reference == 44U ? std::optional<std::uint64_t>{1001U} : std::nullopt;
        },
        .resolve_name = [&](std::string_view name) -> std::optional<std::uint64_t> {
            delivery_trace.push_back("name:" + std::string(name));
            return name == "named" ? std::optional<std::uint64_t>{1002U} : std::nullopt;
        },
        .direct_dispatch = [&](std::uint64_t target, std::uint16_t event, std::uint32_t argument,
                               std::uint64_t sender) {
            delivery_trace.push_back("dispatch"); dispatched_targets.push_back(target);
            check(event == 0x401U && argument == 99U && sender == 77U);
        },
    };
    CommandDeliveryAdapter delivery(std::move(delivery_services), 77U);
    Command due{}; due.event_reference = 7U; due.target_reference = 44U;
    due.target_name = "named"; due.event_argument = 99U;
    check(delivery.deliver(due) == CommandDeliveryResult::delivered &&
          delivery_trace == std::vector<std::string>({"event:7", "reference:44", "dispatch"}) &&
          dispatched_targets == std::vector<std::uint64_t>({1001U}));

    delivery_trace.clear(); dispatched_targets.clear(); due.target_reference = 0U;
    check(delivery.deliver(due) == CommandDeliveryResult::delivered &&
          delivery_trace == std::vector<std::string>({"event:7", "name:named", "dispatch"}) &&
          dispatched_targets == std::vector<std::uint64_t>({1002U}));

    delivery_trace.clear(); due.event_reference = 6U;
    check(delivery.deliver(due) == CommandDeliveryResult::event_unresolved &&
          delivery_trace == std::vector<std::string>({"event:6"}));

    delivery_trace.clear(); due.event_reference = 7U; due.target_reference = 45U;
    check(delivery.deliver(due) == CommandDeliveryResult::target_unresolved &&
          delivery_trace == std::vector<std::string>({"event:7", "reference:45"}));

    delivery_trace.clear(); due.target_reference = 0U; due.target_name.clear();
    check(delivery.deliver(due) == CommandDeliveryResult::no_target &&
          delivery_trace == std::vector<std::string>({"event:7"}));

    rejects([&] { CommandDeliveryAdapter invalid({}, 0U); });

    static_assert(!std::is_copy_constructible_v<FirstCutCommandSession> &&
                  !std::is_move_constructible_v<FirstCutCommandSession>);
    static_assert(std::is_constructible_v<FirstCutCommandSession,
                                          const off::graphics::IntroRuntime&, float,
                                          FirstCutCommandSessionServices, std::uint64_t>);
    std::array<Command, 3> first_cut_commands{};
    first_cut_commands[0] = Command{.timeline_position = 2U, .event_reference = 1U,
                                    .target_reference = 91U, .event_argument = 40U};
    first_cut_commands[1] = Command{.timeline_position = 0U, .event_reference = 2U,
                                    .target_reference = 0U, .event_argument = 41U,
                                    .target_name = "unavailable"};
    first_cut_commands[2] = Command{.timeline_position = 3U, .event_reference = 3U,
                                    .target_reference = 93U, .event_argument = 42U};
    std::array<std::optional<std::uint32_t>, 4> event_mapping{
        std::nullopt, 0x401U, 0x400U, std::nullopt};
    std::vector<std::string> session_trace;
    FirstCutCommandSessionServices session_services{
        .resolve_reference = [&](std::uint32_t reference) -> std::optional<std::uint64_t> {
          session_trace.push_back("reference:" + std::to_string(reference));
          return reference == 91U ? std::optional<std::uint64_t>{991U} : std::nullopt;
        },
        .direct_dispatch = [&](std::uint64_t target, std::uint16_t event,
                               std::uint32_t argument, std::uint64_t sender) {
          session_trace.push_back("dispatch");
          check(target == 991U && event == 0x401U && argument == 40U && sender == 1234U);
        },
    };
    FirstCutCommandSession session(first_cut_commands, event_mapping, 10.0F,
                                   std::move(session_services), 1234U);
    first_cut_commands[0].target_reference = 777U;
    event_mapping[1] = 2U;
    session.run(0.0F);
    check(session_trace.empty());
    session.run(2.5F);
    check(session_trace == std::vector<std::string>({"reference:91", "dispatch"}));
    session_trace.clear();
    session.run(4.0F);
    check(session_trace.empty());
    session.reset_start();
    session_trace.clear();
    session.run(4.0F);
    check(session_trace == std::vector<std::string>({"reference:91", "dispatch"}));
    static_assert(!std::is_copy_constructible_v<off::cutscene::FirstCutClockedCommandRunner> &&
                  !std::is_move_constructible_v<off::cutscene::FirstCutClockedCommandRunner>);
    off::cutscene::FirstCutClockedCommandRunner clocked(session);
    rejects([&] { clocked.update(100U); });
    session_trace.clear();
    clocked.start(100U);
    check(clocked.active() && clocked.scene_clock_start()==100U);
    clocked.update(100U);
    check(session_trace.empty());
    // The recovered converter produces 2.001953125 at this scene-clock delta,
    // which crosses the first command's strict position threshold.
    clocked.update(182U);
    check(session_trace == std::vector<std::string>({"reference:91", "dispatch"}));
    rejects([&] { clocked.start(100U); });
    clocked.stop();
    check(!clocked.active());
    rejects([&] {
      FirstCutCommandSession incomplete(first_cut_commands, event_mapping, 10.0F, {}, 1U);
    });
    session_trace.clear();
    FirstCutCommandSession named_session(
        std::span<const Command>{first_cut_commands}.subspan(1U, 1U), event_mapping, 10.0F,
        FirstCutCommandSessionServices{
            .resolve_reference = [](std::uint32_t) -> std::optional<std::uint64_t> {
              return std::nullopt;
            },
            .resolve_name = [&](std::string_view name) -> std::optional<std::uint64_t> {
              session_trace.push_back("name:" + std::string(name));
              return name == "unavailable" ? std::optional<std::uint64_t>{992U} : std::nullopt;
            },
            .direct_dispatch = [&](std::uint64_t target, std::uint16_t event,
                                   std::uint32_t argument, std::uint64_t sender) {
              session_trace.push_back("named-dispatch");
              check(target == 992U && event == 0x400U && argument == 41U && sender == 1235U);
            },
        },
        1235U);
    named_session.run(1.0F);
    check(session_trace == std::vector<std::string>({"name:unavailable", "named-dispatch"}));

    static_assert(!std::is_copy_constructible_v<IntroLiveTargetRegistry> &&
                  !std::is_move_constructible_v<IntroLiveTargetRegistry>);
    IntroLiveTargetRegistry targets;
    targets.register_owner({.owner = 100U, .authored_reference = 9U, .name = "leader"});
    targets.register_owner({.owner = 200U, .authored_reference = 10U, .name = "sender"});
    targets.register_owner({.owner = 300U, .authored_reference = 11U, .name = ""});
    targets.register_sender(500U);
    targets.register_sender(500U);
    targets.register_component(100U, 101U);
    targets.register_component(100U, 102U, false);
    targets.register_component(100U, 103U);
    check(targets.resolve_reference(9U) == std::optional<std::uint64_t>{100U} &&
          targets.resolve_reference(0U) == std::nullopt &&
          targets.resolve_name("leader") == std::optional<std::uint64_t>{100U} &&
          targets.resolve_name("missing") == std::nullopt && targets.resolve_name("") == std::nullopt);
    rejects([&] { targets.register_owner({.owner = 100U, .authored_reference = 11U, .name = "other"}); });
    rejects([&] { targets.register_owner({.owner = 101U, .authored_reference = 9U, .name = "other"}); });
    rejects([&] { targets.register_owner({.owner = 101U, .authored_reference = 11U, .name = "leader"}); });
    rejects([&] { targets.register_component(999U, 104U); });
    rejects([&] { targets.register_component(100U, 101U); });
    std::vector<std::string> target_trace;
    IntroLiveTargetRegistry::DispatchServices target_services{
        .direct_target = [&](std::uint64_t target, std::uint16_t event, std::uint32_t argument,
                             std::uint64_t sender) {
          target_trace.push_back("target");
          check(target == 100U && event == 0x402U && argument == 55U &&
                (sender == 200U || sender == 500U));
          rejects([&] { targets.register_component(100U, 104U); });
          rejects([&] { targets.dispatch(100U, event, argument, sender, target_services); });
        },
        .direct_component = [&](std::uint64_t target, std::uint64_t component, std::uint16_t,
                                std::uint32_t, std::uint64_t) {
          target_trace.push_back("component:" + std::to_string(component));
          check(target == 100U);
        },
    };
    targets.dispatch(100U, 0x402U, 55U, 200U, target_services);
    check(target_trace == std::vector<std::string>({"target", "component:101", "component:103"}));
    target_trace.clear();
    targets.dispatch(100U, 0x402U, 55U, 500U, target_services);
    check(target_trace == std::vector<std::string>({"target", "component:101", "component:103"}));
    targets.set_component_eligible(102U, true);
    target_trace.clear();
    targets.dispatch(100U, 0x402U, 55U, 200U, target_services);
    check(target_trace == std::vector<std::string>({"target", "component:101", "component:102", "component:103"}));
    targets.unregister_component(102U);
    check(!targets.contains_component(102U));
    target_trace.clear();
    targets.dispatch(100U, 0x402U, 55U, 200U, target_services);
    check(target_trace == std::vector<std::string>({"target", "component:101", "component:103"}));
    // The component sequence is snapshotted after the target hook, then the
    // registry checks each handle again before its callback. Removing 103
    // while 101 is running must therefore skip the stale tail entry.
    target_services.direct_component = [&](std::uint64_t, std::uint64_t component, std::uint16_t,
                                           std::uint32_t, std::uint64_t) {
      target_trace.push_back("component:" + std::to_string(component));
      if (component == 101U) targets.unregister_component(103U);
    };
    target_trace.clear();
    targets.dispatch(100U, 0x402U, 55U, 200U, target_services);
    check(target_trace == std::vector<std::string>({"target", "component:101"}) &&
          !targets.contains_component(103U));
    targets.unregister_owner(100U);
    check(!targets.contains_owner(100U) && !targets.contains_component(101U) &&
          targets.resolve_reference(9U) == std::nullopt && targets.resolve_name("leader") == std::nullopt &&
          targets.contains_owner(500U));
    rejects([&] { targets.dispatch(100U, 0x402U, 55U, 200U, target_services); });
    rejects([&] { targets.dispatch(200U, 0x402U, 55U, 100U, target_services); });
    targets.register_owner({.owner = 400U, .authored_reference = 12U, .name = "ephemeral"});
    targets.register_component(400U, 401U);
    bool removed_target_called = false;
    IntroLiveTargetRegistry::DispatchServices removal_services{
        .direct_target = [&](std::uint64_t target, std::uint16_t, std::uint32_t, std::uint64_t) {
          removed_target_called = true;
          targets.unregister_owner(target);
        },
        .direct_component = [&](std::uint64_t, std::uint64_t, std::uint16_t, std::uint32_t,
                                std::uint64_t) { check(false); },
    };
    targets.dispatch(400U, 0x402U, 55U, 200U, removal_services);
    check(removed_target_called && !targets.contains_owner(400U) &&
          targets.resolve_reference(12U) == std::nullopt);

    ExternalCutSequenceCommand external_command;
    rejects([&] { external_command.run_phase_two({}); });
    std::vector<std::string> external_trace;
    Command external_data{};
    external_data.timeline_position = 385U;
    external_data.event_reference = 198U;
    external_data.target_reference = 0x8000000aU;
    external_data.event_argument = 1000U;
    external_command.read({
        .read_common_command = [&] { external_trace.push_back("common"); return external_data; },
        .read_external_list_reference = [&] { external_trace.push_back("external"); return 0x80000042U; },
    });
    check(external_command.read_complete() && external_command.command().event_reference == 198U &&
          external_command.external_list_reference() == 0x80000042U);
    rejects([&] { external_command.read({}); });
    ExternalCutSequenceCommandPhaseTwoServices external_services{
        .resolve_reference = [&](std::uint32_t ref) -> std::optional<std::uint64_t> {
          external_trace.push_back("reference:" + std::to_string(ref));
          return ref == 0x80000042U ? std::optional<std::uint64_t>{65U} : std::nullopt;
        },
        .find_owner_component = [&](std::uint64_t owner, std::string_view name)
            -> std::optional<std::uint64_t> {
          external_trace.push_back("component:" + std::to_string(owner) + ":" + std::string(name));
          return owner == 65U && name == "CutSequenceList" ? std::optional<std::uint64_t>{84U} : std::nullopt;
        },
        .register_ordered_command = [&](std::uint64_t list, const Command& command) {
          external_trace.push_back("register:" + std::to_string(list));
          check(list == 84U && command.timeline_position == 385U);
        },
        .read_scene_handle = [&](std::string_view name) -> std::optional<std::uint64_t> {
          external_trace.push_back("property:" + std::string(name)); return 77U;
        },
        .resolve_scene_object = [&](std::uint64_t handle) -> std::optional<std::uint64_t> {
          external_trace.push_back("scene:" + std::to_string(handle)); return handle == 77U ? std::optional<std::uint64_t>{900U} : std::nullopt;
        },
        .retire_diagnostic = [&] { external_trace.push_back("retire"); },
    };
    external_command.run_phase_two(external_services);
    check(external_command.phase_two_completed() && !external_command.retired() &&
          external_command.cached_context() == std::optional<std::uint64_t>{900U} &&
          external_trace == std::vector<std::string>({"common", "external", "reference:2147483714",
              "component:65:CutSequenceList", "register:84", "property:rCutSequenceObjects", "scene:77"}));
    rejects([&] { external_command.run_phase_two(external_services); });

    ExternalCutSequenceCommand negative_external;
    Command negative_external_data{};
    negative_external_data.timeline_position = 0xffffffffU;
    negative_external.read({.read_common_command = [&] { return negative_external_data; },
                            .read_external_list_reference = [] { return 9U; }});
    external_trace.clear();
    auto negative_services = external_services;
    negative_services.resolve_reference = [&](std::uint32_t) -> std::optional<std::uint64_t> { external_trace.push_back("reference"); return 65U; };
    negative_services.find_owner_component = [&](std::uint64_t, std::string_view) -> std::optional<std::uint64_t> { external_trace.push_back("component"); return 84U; };
    negative_services.register_ordered_command = [&](std::uint64_t, const Command&) { external_trace.push_back("register"); };
    negative_services.read_scene_handle = [&](std::string_view) -> std::optional<std::uint64_t> { external_trace.push_back("property"); return std::nullopt; };
    negative_services.resolve_scene_object = [&](std::uint64_t value) -> std::optional<std::uint64_t> { external_trace.push_back("scene:" + std::to_string(value)); return std::nullopt; };
    negative_external.run_phase_two(negative_services);
    check(negative_external.phase_two_completed() && !negative_external.cached_context() &&
          external_trace == std::vector<std::string>({"reference", "component", "property", "scene:0"}));

    ExternalCutSequenceCommand missing_external;
    missing_external.read({.read_common_command = [] { return Command{}; },
                           .read_external_list_reference = [] { return 3U; }});
    external_trace.clear();
    auto missing_services = external_services;
    missing_services.resolve_reference = [&](std::uint32_t) -> std::optional<std::uint64_t> { external_trace.push_back("reference"); return std::nullopt; };
    missing_services.retire_diagnostic = [&] { external_trace.push_back("retire"); };
    missing_external.run_phase_two(missing_services);
    check(missing_external.retired() && !missing_external.phase_two_completed() &&
          external_trace == std::vector<std::string>({"reference", "retire"}));
    rejects([&] { missing_external.run_phase_two(missing_services); });

    off::cutscene::CutSequenceList external_list;
    rejects([&] { external_list.register_ordered_command(Command{}); });
    std::vector<std::string> list_trace;
    rejects([&] { external_list.run_phase_one({}); });
    external_list.run_phase_one({
        .source_directory = 65U,
        .source_type = 0x0800001aU,
        .class_data_value = 0U,
        .read_retained_source = [&] { list_trace.push_back("read"); },
    });
    check(external_list.phase_one_complete() && !external_list.phase_two_complete() &&
          list_trace == std::vector<std::string>({"read"}));
    Command list_late{}; list_late.timeline_position = 385U;
    Command early{}; early.timeline_position = 1U;
    Command equal{}; equal.timeline_position = 1U;
    external_list.register_ordered_command(list_late);
    external_list.register_ordered_command(early);
    external_list.register_ordered_command(equal);
    check(external_list.commands().size() == 3U &&
          external_list.commands()[0].timeline_position == 1U &&
          external_list.commands()[1].timeline_position == 1U &&
          external_list.commands()[2].timeline_position == 385U);
    external_list.run_phase_two({.phase_complete = [&] { list_trace.push_back("phase-two"); }});
    check(external_list.phase_two_complete() &&
          list_trace == std::vector<std::string>({"read", "phase-two"}));
    rejects([&] { external_list.register_ordered_command(early); });
    rejects([&] { external_list.run_phase_two({.phase_complete = [] {}}); });

    // Source 466 runs its reverse-order command callbacks while source 65's
    // retained list is phase-one complete and before its phase-two callback.
    off::cutscene::CutSequenceList joined_list;
    std::vector<std::string> join_trace;
    joined_list.run_phase_one({.source_directory = 65U, .source_type = 0x0800001aU,
                               .class_data_value = 0U,
                               .read_retained_source = [&] { join_trace.push_back("list-read"); }});
    off::cutscene::ExternalCutSequenceListJoin join(
        84U, joined_list,
        {.resolve_reference = [&](std::uint32_t reference) -> std::optional<std::uint64_t> {
             join_trace.push_back("reference:" + std::to_string(reference));
             return reference == 0x80000042U ? std::optional<std::uint64_t>{65U} : std::nullopt;
         },
         .find_owner_component = [&](std::uint64_t owner, std::string_view name)
             -> std::optional<std::uint64_t> {
             join_trace.push_back("component:" + std::to_string(owner) + ":" + std::string(name));
             return owner == 65U && name == "CutSequenceList" ? std::optional<std::uint64_t>{84U} : std::nullopt;
         },
         .read_scene_handle = [&](std::string_view name) -> std::optional<std::uint64_t> {
             join_trace.push_back("property:" + std::string(name)); return std::nullopt;
         },
         .resolve_scene_object = [&](std::uint64_t handle) -> std::optional<std::uint64_t> {
             join_trace.push_back("scene:" + std::to_string(handle)); return std::nullopt;
         },
         .retire_diagnostic = [&] { join_trace.push_back("retire"); }});
    ExternalCutSequenceCommand late_external;
    ExternalCutSequenceCommand early_external;
    Command joined_late{}; joined_late.timeline_position = 385U;
    Command joined_early{}; joined_early.timeline_position = 1U;
    late_external.read({.read_common_command = [&] { return joined_late; },
                        .read_external_list_reference = [] { return 0x80000042U; }});
    early_external.read({.read_common_command = [&] { return joined_early; },
                         .read_external_list_reference = [] { return 0x80000042U; }});
    join.run_phase_two(late_external);
    join.run_phase_two(early_external);
    check(joined_list.commands().size() == 2U &&
              joined_list.commands()[0].timeline_position == 1U &&
              joined_list.commands()[1].timeline_position == 385U &&
              !late_external.cached_context() && !early_external.cached_context() &&
              join_trace == std::vector<std::string>({"list-read", "reference:2147483714",
                  "component:65:CutSequenceList", "property:rCutSequenceObjects", "scene:0",
                  "reference:2147483714", "component:65:CutSequenceList",
                  "property:rCutSequenceObjects", "scene:0"}));
    joined_list.run_phase_two({.phase_complete = [&] { join_trace.push_back("list-phase-two"); }});
    rejects([&] { join.run_phase_two(early_external); });

    off::cutscene::CutSequenceList rejected_list;
    rejects([&] { off::cutscene::ExternalCutSequenceListJoin invalid_join(84U, rejected_list, {}); });
    return failures == 0 ? 0 : 1;
}
