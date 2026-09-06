#include "off/graphics/renderer_frame_pass.hpp"
#include "off/graphics/admitted_view_pass.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace off::graphics;
using Trace = std::vector<std::pair<char, std::uint64_t>>;
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported renderer frame must reject");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<RendererFramePass> &&
                  !std::is_move_constructible_v<RendererFramePass> &&
                  !std::is_copy_assignable_v<RendererFramePass> &&
                  !std::is_move_assignable_v<RendererFramePass>);
    RendererFramePass pass;
    std::vector<RendererStateEntry> states{{7,20},{8,30},{7,0},{7,20}};
    Trace trace;
    const RendererFrameHooks hooks{
        [&](std::uint64_t state) { trace.emplace_back('F', state); },
        [&](std::uint64_t state) { trace.emplace_back('M', state); },
        [&] { trace.emplace_back('B', 0); }};
    pass.run(false, true, 7, states, {});
    pass.run(false, false, 7, states, hooks);
    check(trace.empty(), "unadmitted renderer has no frame or maintenance effects and needs no hooks");
    pass.run(true, false, 7, states, hooks);
    check(trace == Trace{{'B',0}}, "unready backend still reaches admitted renderer maintenance");
    trace.clear(); pass.run(true, true, 7, states, hooks);
    const Trace full{{'F',20},{'F',0},{'F',20},{'M',20},{'M',0},{'M',20},{'B',0}};
    check(trace == full, "renderer identity filter preserves storage order, duplicate states and zero identities");
    trace.clear(); pass.run(true, true, 99, states, hooks);
    check(trace == Trace{{'B',0}}, "no matching states does not invent a state frame");
    trace.clear(); pass.run(true, true, 7, {}, hooks);
    check(trace == Trace{{'B',0}}, "empty state collection still allows backend maintenance");
    for (std::size_t missing = 0; missing < 3; ++missing) {
        auto invalid = hooks;
        if (missing == 0) invalid.state_frame = {};
        if (missing == 1) invalid.state_maintenance = {};
        if (missing == 2) invalid.backend_maintenance = {};
        trace.clear();
        rejects([&] { pass.run(true, true, 7, states, invalid); });
        rejects([&] { pass.run(true, false, 7, states, invalid); });
        check(trace.empty(), "required hooks validated before effects even when backend is unready");
    }
    trace.clear();
    bool changed = false;
    const RendererFrameHooks mutate_registration{
        [&](std::uint64_t state) {
            trace.emplace_back('F', state);
            if (!changed) {
                changed = true;
                std::vector<RendererStateEntry>{{7,999}}.swap(states);
            }
        }, hooks.state_maintenance, hooks.backend_maintenance};
    pass.run(true, true, 7, states, mutate_registration);
    check(trace == full, "snapshot survives registration storage replacement for both frame and maintenance phases");
    trace.clear(); pass.run(true, true, 7, states, hooks);
    check(trace == Trace{{'F',999},{'M',999},{'B',0}}, "later pass observes changed registration collection");
    states = {{7,20},{7,40}};
    for (const auto failing_phase : {'F','M','B'}) {
        trace.clear();
        const RendererFrameHooks throwing{
            [&](std::uint64_t state) {
                trace.emplace_back('F', state);
                if (failing_phase == 'F') throw std::runtime_error("frame failure");
            },
            [&](std::uint64_t state) {
                trace.emplace_back('M', state);
                if (failing_phase == 'M') throw std::runtime_error("state maintenance failure");
            },
            [&] { trace.emplace_back('B', 0); throw std::runtime_error("backend maintenance failure"); }};
        rejects([&] { pass.run(true, true, 7, states, throwing); });
        Trace expected{{'F',20}};
        if (failing_phase != 'F') {
            expected.emplace_back('F',40); expected.emplace_back('M',20);
        }
        if (failing_phase == 'B') {
            expected.emplace_back('M',40); expected.emplace_back('B',0);
        }
        check(trace == expected, "failure retains prefix without forced later cleanup");
        trace.clear(); pass.run(true, true, 7, states, hooks);
        check(trace == Trace{{'F',20},{'F',40},{'M',20},{'M',40},{'B',0}}, "failure releases guard and retry repeats full frame");
    }
    trace.clear();
    const RendererFrameHooks recursive{
        [&](std::uint64_t state) {
            rejects([&] { pass.run(false, false, 7, {}, {}); });
            rejects([&] { pass.run(true, true, 7, states, hooks); });
            trace.emplace_back('F',state);
        },
        [&](std::uint64_t state) {
            rejects([&] { pass.run(true, false, 7, states, hooks); });
            trace.emplace_back('M',state);
        },
        [&] { rejects([&] { pass.run(true, true, 7, states, hooks); }); trace.emplace_back('B',0); }};
    pass.run(true, true, 7, states, recursive);
    check(trace == Trace{{'F',20},{'F',40},{'M',20},{'M',40},{'B',0}}, "caught recursion preserves outer operation");
    auto uncaught = hooks;
    uncaught.state_frame = [&](std::uint64_t) { pass.run(true, true, 7, states, hooks); };
    rejects([&] { pass.run(true, true, 7, states, uncaught); });
    trace.clear(); pass.run(true, false, 7, states, hooks);
    check(trace == Trace{{'B',0}}, "uncaught recursion releases guard");

    // Explicit local identities and admission, not recovered startup readiness.
    const std::array<AdmittedView, 2> views{{{50,9},{60,1}}};
    const std::array<ViewCameraState, 2> cameras{{{500,true},{600,true}}};
    AdmittedViewPass view_pass(views);
    const std::array<RendererStateEntry, 2> nested_states{{{7,80},{7,90}}};
    trace.clear();
    pass.run(true, true, 7, nested_states, {
        [&](std::uint64_t state) {
            trace.emplace_back('F',state);
            view_pass.run(cameras, [&] { trace.emplace_back('P',state); },
                [&](AdmittedViewPass::Stage stage, const AdmittedView& view, const auto&, std::size_t) {
                    const auto label = stage == AdmittedViewPass::Stage::begin ? 'b' :
                        stage == AdmittedViewPass::Stage::traverse ? 't' : 'e';
                    trace.emplace_back(label, view.identity);
                });
        }, hooks.state_maintenance, hooks.backend_maintenance});
    check(trace == Trace{{'F',80},{'P',80},{'b',60},{'t',60},{'e',60},{'b',50},{'t',50},{'e',50},
                         {'F',90},{'P',90},{'b',60},{'t',60},{'e',60},{'b',50},{'t',50},{'e',50},
                         {'M',80},{'M',90},{'B',0}},
          "outer snapshots compose with ordered admitted views before separate maintenance pass");
    states = {{7,20},{8,30},{7,0},{7,20}};
    changed = false;
    trace.clear();
    pass.run_and_draw(true, true, 7, states, mutate_registration,
        [&](std::span<const std::uint64_t> snapshot) {
            check(trace == full, "ordered drawing follows every preparation and maintenance callback");
            check(std::vector<std::uint64_t>(snapshot.begin(), snapshot.end()) ==
                  std::vector<std::uint64_t>{20,0,20},
                  "ordered drawing receives identical snapshot despite replaced registration storage");
            rejects([&] { pass.run(false, false, 7, {}, {}); });
            trace.emplace_back('D', snapshot.size());
        });
    auto with_drawing = full; with_drawing.emplace_back('D',3);
    check(trace == with_drawing, "duplicate and zero snapshot identities survive into ordered drawing");
    trace.clear();
    pass.run_and_draw(true, false, 7, states, hooks, [&](auto snapshot) {
        check(snapshot.empty() && trace == Trace{{'B',0}}, "unready backend provides empty snapshot after maintenance");
        trace.emplace_back('D',0);
    });
    check(trace == Trace{{'B',0},{'D',0}}, "empty ordered coordinator still has an explicit invocation");
    trace.clear();
    rejects([&] { pass.run_and_draw(true, true, 7, states, hooks, {}); });
    check(trace.empty(), "missing ordered-drawing service rejects before effects");
    rejects([&] { pass.run_and_draw(true, true, 7, states, hooks, [&](auto) {
        trace.emplace_back('D',0); throw std::runtime_error("drawing failure");
    }); });
    check(trace == Trace{{'F',999},{'M',999},{'B',0},{'D',0}}, "drawing failure preserves preparation prefix");
    return failures == 0 ? 0 : 1;
}
