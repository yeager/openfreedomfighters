#include "off/platform/startup_lifecycle.hpp"
#include "off/runtime/startup_boot_menu_admission.hpp"
#include "off/runtime/startloader_load_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace std::chrono_literals;
  const off::platform::StartupClock::time_point origin{123s};

  off::platform::StartupLifecycle ready_early;
  check(ready_early.tick(origin + 20s, true) ==
            off::platform::StartupPhase::awaiting_first_presentation,
        "work completion cannot start the splash deadline");
  ready_early.presented(origin);
  check(ready_early.tick(origin + 2999ms, true) ==
            off::platform::StartupPhase::splash,
        "keep an early result behind the full splash duration");
  check(ready_early.tick(origin + 3s, true) ==
            off::platform::StartupPhase::ready,
        "release an early result at the exact deadline");

  off::platform::StartupLifecycle ready_late;
  ready_late.presented(origin);
  ready_late.presented(origin + 2s);
  check(ready_late.tick(origin + 3s, false) ==
            off::platform::StartupPhase::loading,
        "remove the splash at its deadline while work continues");
  check(ready_late.tick(origin + 4s, true) ==
            off::platform::StartupPhase::ready,
        "release a result completed after the deadline");

  off::platform::StartupLifecycle cancelled;
  cancelled.presented(origin);
  cancelled.cancel();
  check(cancelled.tick(origin + 30s, true) ==
            off::platform::StartupPhase::cancelled,
        "cancellation is terminal");

  off::platform::StartupLifecycle near_clock_limit;
  const auto near_max = off::platform::StartupClock::time_point::max() - 1s;
  near_clock_limit.presented(near_max);
  check(
      near_clock_limit.tick(near_max, true) ==
              off::platform::StartupPhase::splash &&
          near_clock_limit.tick(off::platform::StartupClock::time_point::max(),
                                true) == off::platform::StartupPhase::ready,
      "saturate a deadline that would exceed the clock range");

  bool rejected = false;
  try {
    static_cast<void>(off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
        "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", false));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, "LoadScreen source requires a caller-proven exact wrapper");
  rejected = false;
  try {
    static_cast<void>(off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
        "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", true));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, "LoadScreen source rejects a non-supported target");

  const auto source = off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
      "ZWINGROUP_LoadScreen", 0.0F, "FF-Startup", true);
  off::runtime::SceneTransitionQueue transitions;
  transitions.retain_scene_entry(11U);
  transitions.retain_scene_entry(12U);
  transitions.set_current_scene(11U);
  off::runtime::LoadScreenTransition load_screen(source);
  bool setup_missing = false;
  try {
    static_cast<void>(load_screen.ordinary_update(transitions));
  } catch (const std::runtime_error&) {
    setup_missing = true;
  }
  check(setup_missing && load_screen.one_time_setup_pending() &&
            load_screen.update_count() == 0U,
        "a missing setup service preserves LoadScreen state");
  std::uint32_t setup_calls{};
  const auto setup = [&] { ++setup_calls; };
  check(!load_screen.ordinary_update(transitions, setup) &&
            !load_screen.ordinary_update(transitions, setup) &&
            setup_calls == 1U && transitions.targets().empty(),
        "LoadScreen setup runs once and the first two updates retain no target");
  check(load_screen.ordinary_update(transitions, setup) && setup_calls == 1U &&
            transitions.clear_requests() == 1U && transitions.pending() &&
            !transitions.current_scene().has_value() && transitions.targets().size() == 1U &&
            transitions.targets().front() == "FF-Startup" &&
            load_screen.retained_target().empty(),
        "third LoadScreen update clears then queues its retained target");
  check(transitions.entries().size() == 2U &&
            transitions.entries()[0].removal_requested &&
            transitions.entries()[1].removal_requested,
        "clear request marks retained entries without consuming them");
  check(!load_screen.ordinary_update(transitions, setup) &&
            transitions.clear_requests() == 2U && transitions.targets().size() == 1U,
        "post-request updates retain native empty-target behavior without loading a scene");

  off::runtime::SceneTransitionQueue slash_queue;
  check(slash_queue.request_target("Scenes/FF-StartUp") &&
            slash_queue.targets().front() == "Scenes\\FF-StartUp",
        "target requests retain a copied slash-normalized path");

  off::runtime::StartupBootMenuAdmission boot_menu;
  const off::runtime::StartupBootMenuSource boot_source{91U, 101U, 102U};
  std::uint32_t resolve_calls{};
  std::uint64_t initialized_owner{};
  std::uint16_t initialized_route{};
  std::uint16_t retained_action{};
  const off::runtime::StartupBootMenuAdmissionServices boot_services{
      .event_registry_live = [] { return true; },
      .resolve_event = [&](std::uint64_t identity)
          -> std::optional<std::uint16_t> {
        ++resolve_calls;
        if (identity == 101U) return 31U;
        if (identity == 102U) return 32U;
        return std::nullopt;
      },
      .live_window_owner = [](std::uint64_t owner) { return owner == 91U; },
      .initialize_window = [&](std::uint64_t owner, std::uint16_t route) {
        initialized_owner = owner;
        initialized_route = route;
        return true;
      },
      .action_map_live = [] { return true; },
      .retain_action = [&](std::uint16_t action) {
        retained_action = action;
        return true;
      },
  };
  boot_menu.initialize(boot_source, boot_services);
  check(boot_menu.interactive() && !boot_menu.failed() && resolve_calls == 2U &&
            initialized_owner == 91U && initialized_route == 32U &&
            retained_action == 31U && boot_menu.action_id() == 31U &&
            boot_menu.routing_id() == 32U,
        "boot-menu admission retains only live opaque action and routing IDs");
  check(!boot_menu.observe({32U}) && boot_menu.observe({31U}) &&
            boot_menu.observed_actions() == 1U,
        "boot-menu observation records the resolved action without a selection side effect");

  off::runtime::StartupBootMenuAdmission missing_map;
  auto unavailable_map_services = boot_services;
  unavailable_map_services.action_map_live = [] { return false; };
  rejected = false;
  try {
    missing_map.initialize(boot_source, unavailable_map_services);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected && missing_map.failed() && !missing_map.interactive(),
        "boot-menu admission fails closed when the caller-owned action map is absent");

  std::cout << "startup lifecycle tests passed\n";
}
