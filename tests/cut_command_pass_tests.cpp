#include "off/cutscene/command_pass.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <iostream>

namespace {
using off::cutscene::CommandPass;
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
    return failures == 0 ? 0 : 1;
}
