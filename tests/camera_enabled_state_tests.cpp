#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/admitted_view_pass.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using off::graphics::CameraEnabledState;
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported enabled-state operation must reject");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<CameraEnabledState> &&
                  !std::is_move_constructible_v<CameraEnabledState> &&
                  !std::is_copy_assignable_v<CameraEnabledState> &&
                  !std::is_move_assignable_v<CameraEnabledState>);
    constexpr std::uint32_t unrelated = 0xabcdefd5U; // Enabled bit clear, every other bit explicit.
    static_assert((unrelated & 0x20U) == 0U);
    CameraEnabledState state(unrelated);
    check(state.flags() == unrelated && !state.enabled(), "constructor preserves supplied runtime flags without inferred enabled default");
    unsigned notifications = 0;
    state.set_enabled(false, true, {});
    state.set_enabled(false, true, [&] { ++notifications; });
    check(notifications == 0 && state.flags() == unrelated, "idempotent disabled request does not require or call renderer hook");
    rejects([&] { state.set_enabled(true, true, {}); });
    check(state.flags() == unrelated, "missing transition hook rejects before bit store");
    state.set_enabled(true, true, [&] {
        ++notifications;
        check(!state.enabled() && state.flags() == unrelated, "enable callback sees pre-transition runtime flags");
    });
    check(state.enabled() && state.flags() == (unrelated | 0x20U) && notifications == 1,
          "enable changes only enabled bit after notification");
    state.set_enabled(true, true, {});
    state.set_enabled(true, true, [&] { ++notifications; });
    check(notifications == 1, "idempotent enable does not notify");
    state.set_enabled(false, true, [&] {
        ++notifications;
        check(state.enabled() && state.flags() == (unrelated | 0x20U), "disable callback sees enabled bit before clear");
    });
    check(state.flags() == unrelated && notifications == 2, "disable preserves unrelated bits");
    state.set_enabled(true, false, [&] { ++notifications; });
    check(state.enabled() && notifications == 2, "absent renderer does not call even a supplied hook");
    state.set_enabled(false, false, {});
    check(!state.enabled() && state.flags() == unrelated, "absent renderer transition needs no hook");

    for (const bool requested : {true, false}) {
        state.set_enabled(!requested, false, {});
        const auto previous = state.flags();
        unsigned attempts = 0;
        rejects([&] { state.set_enabled(requested, true, [&] {
            ++attempts;
            check(state.flags() == previous, "throwing hook observes unchanged flags");
            throw std::runtime_error("renderer state failure");
        }); });
        check(state.flags() == previous && attempts == 1, "hook throw retains previous enabled state and external prefix");
        state.set_enabled(requested, true, [&] { ++attempts; });
        check(state.enabled() == requested && attempts == 2, "retry invokes hook again after exception releases guard");
    }
    state.set_enabled(false, false, {});
    state.set_enabled(true, true, [&] {
        rejects([&] { state.set_enabled(false, false, {}); }); // Even apparently idempotent nested request is unsupported.
        rejects([&] { state.set_enabled(true, false, {}); });
        check(!state.enabled(), "caught recursive transitions leave outer old state intact");
    });
    check(state.enabled(), "outer transition commits after caught recursion");
    rejects([&] { state.set_enabled(false, true, [&] { state.set_enabled(false, false, {}); }); });
    check(state.enabled(), "uncaught recursive failure prevents following enabled-bit clear");
    state.set_enabled(false, false, {});
    check(!state.enabled(), "guard releases after recursive failure");
    CameraEnabledState all_bits(0xffffffffU);
    all_bits.set_enabled(false, false, {});
    check(all_bits.flags() == 0xffffffdfU, "disable does not erase other runtime capability flags");
    CameraEnabledState no_bits(0U);
    no_bits.set_enabled(true, false, {});
    check(no_bits.flags() == 0x20U, "enable does not manufacture unrelated runtime flags");

    // Explicit admission remains separate; the view pass consumes the runtime query result.
    const std::array<off::graphics::AdmittedView, 1> views{{{7,11}}};
    off::graphics::AdmittedViewPass view_pass(views);
    std::array<off::graphics::ViewCameraState, 1> cameras{{{9,state.enabled()}}};
    std::vector<off::graphics::AdmittedViewPass::Stage> stages;
    const auto visit = [&](auto stage, const auto&, const auto&, std::size_t) { stages.push_back(stage); };
    view_pass.run(cameras, [] {}, visit);
    check(stages.empty(), "disabled explicit camera query suppresses admitted view callbacks");
    state.set_enabled(true, false, {});
    cameras[0].enabled = state.enabled();
    view_pass.run(cameras, [] {}, visit);
    using Stage = off::graphics::AdmittedViewPass::Stage;
    check(stages == std::vector<Stage>{Stage::begin, Stage::traverse, Stage::end},
          "same explicitly admitted view consumes changed runtime enabled state");
    return failures == 0 ? 0 : 1;
}
