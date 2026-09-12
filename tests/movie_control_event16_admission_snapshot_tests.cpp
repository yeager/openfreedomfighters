#include "off/graphics/movie_control_event16_admission_snapshot.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace off;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
  runtime::SceneComponentSequence sequence{[] { return std::uint32_t{}; }};
  runtime::ComponentLifecycle lifecycle{sequence};
  runtime::OrdinarySortingState sorting;
  runtime::OrdinaryComponentManager manager{sorting, {
      [this](std::uint64_t handle) -> runtime::ComponentRecord* {
        return handle == 1 ? &lifecycle.at(0) : nullptr;
      }, {}, {}}};
  Fixture() {
    const auto index = lifecycle.append({1, {}, {}, "ZGEOM_MovieControl", 0, 0, 0, false});
    lifecycle.construct(index, [](runtime::ComponentRecord&) {
      return runtime::ConstructedComponent{{1, 0, 1, 0x10, 0, 4, 0, 1},
                                           [](auto&) {}, [](auto&) {}};
    });
  }
};
}
int main() {
  try {
    Fixture fixture;
    const auto cold = graphics::MovieControlEvent16ManagerSnapshot::capture(
        fixture.manager, fixture.lifecycle.at(0), 1, 77, false, {});
    check(cold.component_is_live && !cold.event16_enrolled && cold.phase_one_completed,
          "unenrolled component remains a cold manager snapshot");
    fixture.manager.enqueue(1); fixture.manager.refresh();
    const auto admitted = graphics::MovieControlEvent16ManagerSnapshot::capture(
        fixture.manager, fixture.lifecycle.at(0), 1, -19, true, std::uint64_t{9});
    const auto event = admitted.bind({});
    check(event.component_is_live() && event.event16_enrolled() && event.paused() &&
              event.captured_component_filter() == std::optional<std::uint64_t>{9} &&
              event.phase_one_completed() && event.scene_integer_clock() == -19,
          "snapshot freezes only manager admission inputs");
    fixture.lifecycle.at(0).state().status = 0;
    const auto phase_one_pending = graphics::MovieControlEvent16ManagerSnapshot::capture(
        fixture.manager, fixture.lifecycle.at(0), 1, 0, false, {});
    check(!phase_one_pending.phase_one_completed,
          "phase-one status is not fabricated by the snapshot");
    bool rejected = false;
    try { static_cast<void>(graphics::MovieControlEvent16ManagerSnapshot::capture(
        fixture.manager, fixture.lifecycle.at(0), 2, 0, false, {})); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "mismatched common identity cannot become a manager snapshot");
    std::cout << "movie control event16 admission snapshot tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
  }
}
