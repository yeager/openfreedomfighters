#include "off/cutscene/picture_activation_prefix.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using Prefix = off::cutscene::PictureActivationPrefix;
using Stage = Prefix::Stage;
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported activation operation must throw");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<Prefix> && !std::is_move_constructible_v<Prefix> &&
                  !std::is_copy_assignable_v<Prefix> && !std::is_move_assignable_v<Prefix>);
    Prefix prefix;
    std::uint32_t flags = 0x400U | 0x100U | 0x40000U;
    std::vector<Stage> trace;
    const auto record = [&](Stage stage) { trace.push_back(stage); };
    prefix.run(flags, 0U, true, true, [&](Stage stage) {
        check(((flags & 0x400U) != 0U) == (stage == Stage::append_tracking),
              "tracking append precedes clear, helper and lifecycle callbacks see cleared flag");
        record(stage);
    });
    const std::vector<Stage> full{Stage::append_tracking, Stage::class_registration, Stage::owner_activation,
                                 Stage::normal_registration, Stage::phase_one, Stage::record_requested};
    check(trace == full && flags == (0x100U | 0x40000U), "plain-picture activation clears only authored-hide bit");

    trace.clear(); flags = 0x400U | 0x800U | 0x123U;
    const auto blocked_flags = flags;
    prefix.run(flags, 0x400U, true, true, record);
    check(trace == std::vector<Stage>{Stage::append_tracking, Stage::parent_blocked, Stage::phase_one,
                                    Stage::record_requested} && flags == blocked_flags,
          "blocked parent still yields outer phase-one opportunity and requested marker, not success");
    for (const auto remaining_gate : {0x800U, 0x200000U, 0x200800U}) {
        trace.clear(); flags = 0x400U | remaining_gate;
        prefix.run(flags, 0U, true, true, record);
        check(trace == std::vector<Stage>{Stage::append_tracking, Stage::owner_activation,
                                        Stage::phase_one, Stage::record_requested} && flags == remaining_gate,
              "other runtime eligibility flags survive clear and suppress registration helpers");
    }
    trace.clear(); flags = 0x400U;
    prefix.run(flags, 0U, false, true, record);
    check(trace == std::vector<Stage>{Stage::append_tracking, Stage::owner_activation, Stage::normal_registration,
                                    Stage::phase_one, Stage::record_requested},
          "registration-class condition gates only the class helper");
    trace.clear(); flags = 0x400U;
    prefix.run(flags, 0U, true, false, record);
    check(trace == std::vector<Stage>{Stage::append_tracking, Stage::class_registration, Stage::normal_registration,
                                    Stage::phase_one, Stage::record_requested},
          "missing owner skips its notification without fabricating other behavior");
    trace.clear(); flags = 0x800U | 0x200000U;
    prefix.run(flags, std::nullopt, true, true, record);
    check(trace == std::vector<Stage>{Stage::append_tracking, Stage::record_not_requested} &&
          flags == (0x800U | 0x200000U), "already-clear target needs no parent and performs no hide or phase-one pair");
    trace.clear(); flags = 0x400U;
    rejects([&] { prefix.run(flags, std::nullopt, true, true, record); });
    rejects([&] { prefix.run(flags, 0U, true, true, {}); });
    check(trace.empty() && flags == 0x400U, "missing required inputs reject before tracking or flag mutation");

    for (const auto failure_stage : full) {
        trace.clear(); flags = 0x400U;
        rejects([&] { prefix.run(flags, 0U, true, true, [&](Stage stage) {
            record(stage);
            if (stage == failure_stage) throw std::runtime_error("callback failure");
        }); });
        std::vector<Stage> expected;
        for (const auto stage : full) {
            expected.push_back(stage);
            if (stage == failure_stage) break;
        }
        check(trace == expected && flags == (failure_stage == Stage::append_tracking ? 0x400U : 0U),
              "exceptions retain only completed flag and callback prefix effects");
        trace.clear();
        prefix.run(flags, 0U, true, true, record);
        check(trace == (failure_stage == Stage::append_tracking ? full :
                           std::vector<Stage>{Stage::append_tracking, Stage::record_not_requested}),
              "retry uses current flags and released guard, not invented transactional restoration");
    }
    flags = 0x400U; trace.clear();
    rejects([&] { prefix.run(flags, 0x400U, true, true, [&](Stage stage) {
        record(stage);
        if (stage == Stage::parent_blocked) throw std::runtime_error("diagnostic failure");
    }); });
    check(flags == 0x400U && trace == std::vector<Stage>{Stage::append_tracking, Stage::parent_blocked},
          "blocked-parent diagnostic exception does not force later lifecycle calls");
    flags = 0x400U; trace.clear();
    prefix.run(flags, 0U, true, true, [&](Stage stage) {
        rejects([&] { prefix.run(flags, 0U, false, false, record); });
        record(stage);
    });
    check(trace == full && flags == 0U, "caught reentry cannot mutate outer activation");
    flags = 0x400U;
    rejects([&] { prefix.run(flags, 0U, true, true, [&](Stage) {
        prefix.run(flags, 0U, true, true, record);
    }); });
    check(flags == 0x400U, "uncaught append-stage reentry prevents following hide clear");
    trace.clear(); prefix.run(flags, 0U, true, true, record);
    check(trace == full, "guard releases after recursive exception");
    return failures == 0 ? 0 : 1;
}
