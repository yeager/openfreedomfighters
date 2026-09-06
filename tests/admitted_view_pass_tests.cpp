#include "off/graphics/admitted_view_pass.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using off::graphics::AdmittedView;
using off::graphics::AdmittedViewPass;
using off::graphics::ViewCameraState;
using Stage = AdmittedViewPass::Stage;
using Trace = std::vector<std::pair<Stage, std::size_t>>;
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported view operation must reject");
}
Trace expected_trace(std::span<const std::size_t> order) {
    Trace result;
    for (const auto index : order) {
        result.emplace_back(Stage::begin, index);
        result.emplace_back(Stage::traverse, index);
        result.emplace_back(Stage::end, index);
    }
    return result;
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<AdmittedViewPass> &&
                  !std::is_move_constructible_v<AdmittedViewPass> &&
                  !std::is_copy_assignable_v<AdmittedViewPass> &&
                  !std::is_move_assignable_v<AdmittedViewPass>);
    std::array<AdmittedView, 7> views{{{0,5}, {10,1}, {0,5}, {30,0x80000000U},
                                     {40,0xffffffffU}, {50,0}, {60,0x7fffffffU}}};
    const auto original_views = views;
    AdmittedViewPass pass(views);
    for (auto& view : views) { view.priority = 0; view.identity = 999; }
    std::array<ViewCameraState, 7> cameras{};
    for (std::size_t i = 0; i < cameras.size(); ++i) cameras[i] = {i, true};
    Trace trace;
    int frame_count = 0;
    bool frame_started = false;
    const auto begin_frame = [&] { ++frame_count; frame_started = true; };
    const AdmittedViewPass::Visitor record = [&](Stage stage, const AdmittedView& view,
                                                const ViewCameraState& camera, std::size_t index) {
        check(frame_started, "frame bookkeeping precedes every view callback");
        check(view.identity == original_views[index].identity && view.priority == original_views[index].priority,
              "pass owns cached admitted view fields, not mutable caller registrations");
        check(&camera == &cameras[index], "camera state is consumed from explicit stable frame inputs");
        trace.emplace_back(stage, index);
    };
    constexpr std::array<std::size_t, 7> full_order{4,5,1,0,2,6,3};
    pass.run(cameras, begin_frame, record);
    check(frame_count == 1 && trace == expected_trace(full_order),
          "signed negated keys, minimum wrap, stable ties and repeated identity all preserved");
    cameras[5].camera_identity.reset(); cameras[1].enabled = false;
    trace.clear(); frame_started = false;
    pass.run(cameras, begin_frame, record);
    constexpr std::array<std::size_t, 5> filtered{4,0,2,6,3};
    check(frame_count == 2 && trace == expected_trace(filtered),
          "null or disabled cameras skip but zero camera/view identities remain valid");
    trace.clear(); frame_started = false;
    rejects([&] { pass.run(std::span<const ViewCameraState>(cameras).first(6), begin_frame, record); });
    rejects([&] { pass.run(cameras, {}, record); });
    rejects([&] { pass.run(cameras, begin_frame, {}); });
    check(frame_count == 2 && trace.empty(), "all malformed frame inputs reject before external effects");
    rejects([&] { pass.run(cameras, [&] { ++frame_count; throw std::runtime_error("frame"); }, record); });
    check(frame_count == 3 && trace.empty(), "frame-begin exception does not start view callbacks");
    for (const auto throw_stage : {Stage::begin, Stage::traverse, Stage::end}) {
        trace.clear();
        rejects([&] { pass.run(cameras, begin_frame, [&](Stage stage, const auto& view, const auto& camera, auto index) {
            record(stage, view, camera, index);
            if (stage == throw_stage) throw std::runtime_error("view callback");
        }); });
        Trace expected{{Stage::begin, 4}};
        if (throw_stage != Stage::begin) expected.emplace_back(Stage::traverse, 4);
        if (throw_stage == Stage::end) expected.emplace_back(Stage::end, 4);
        check(trace == expected, "callback failure retains prefix without invented end or later views");
        trace.clear(); pass.run(cameras, begin_frame, record);
        check(trace == expected_trace(filtered), "retry after exception starts full pass with guard released");
    }
    trace.clear();
    pass.run(cameras, [&] {
        begin_frame(); rejects([&] { pass.run(cameras, begin_frame, record); });
    }, [&](Stage stage, const auto& view, const auto& camera, auto index) {
        rejects([&] { pass.run(cameras, begin_frame, record); });
        record(stage, view, camera, index);
    });
    check(trace == expected_trace(filtered), "caught recursive calls leave outer pass intact");
    rejects([&] { pass.run(cameras, begin_frame, [&](Stage, const auto&, const auto&, auto) {
        pass.run(cameras, begin_frame, record);
    }); });
    trace.clear(); pass.run(cameras, begin_frame, record);
    check(trace == expected_trace(filtered), "uncaught recursive exception releases guard");

    const std::array<AdmittedView, 3> exact_integer_keys{{{1,16777217U},{2,16777216U},{3,16777217U}}};
    const std::array<ViewCameraState, 3> live{{{1,true},{2,true},{3,true}}};
    AdmittedViewPass integer_pass(exact_integer_keys);
    std::vector<std::size_t> integer_order;
    integer_pass.run(live, [] {}, [&](Stage stage, const auto&, const auto&, auto index) {
        if (stage == Stage::begin) integer_order.push_back(index);
    });
    check(integer_order == std::vector<std::size_t>{1,0,2}, "integer priority keys must not collide through float rounding");
    std::array<AdmittedView, 16> capacity_views{};
    std::array<ViewCameraState, 16> capacity_cameras{};
    for (auto& camera : capacity_cameras) camera = {0, true};
    AdmittedViewPass maximum(capacity_views);
    std::vector<std::size_t> capacity_order;
    maximum.run(capacity_cameras, [] {}, [&](Stage stage, const auto&, const auto&, auto index) {
        if (stage == Stage::begin) capacity_order.push_back(index);
    });
    check(capacity_order.size() == 16, "sixteen views admitted without identity deduplication");
    for (std::size_t i = 0; i < capacity_order.size(); ++i)
        check(capacity_order[i] == i, "equal-key full array remains stable");
    const std::array<AdmittedView, 17> overflow{};
    rejects([&] { AdmittedViewPass invalid(overflow); });
    AdmittedViewPass empty({});
    bool empty_prepared = false;
    empty.run({}, [&] { empty_prepared = true; }, [&](Stage, const auto&, const auto&, auto) {
        check(false, "empty pass has no view callbacks");
    });
    check(empty_prepared, "empty pass still performs explicit frame preparation");
    for (auto& camera : cameras) camera.enabled = false;
    trace.clear(); pass.run(cameras, begin_frame, record);
    check(trace.empty(), "no enabled cameras does not invent a selected winner");
    return failures == 0 ? 0 : 1;
}
