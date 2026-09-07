#include "off/cutscene/command_pass.hpp"

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
    return failures == 0 ? 0 : 1;
}
