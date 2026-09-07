#include "off/platform/startup_lifecycle.hpp"
#include "off/runtime/startup_boot_menu_admission.hpp"
#include "off/runtime/startloader_load_screen.hpp"
#include "off/runtime/startup_scene_loader.hpp"

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

  off::runtime::SceneTransitionQueue pump_queue;
  pump_queue.retain_scene_entry(31U);
  pump_queue.retain_scene_entry(32U);
  pump_queue.set_current_scene(31U);
  pump_queue.request_clear();
  check(pump_queue.request_target("FF-Startup"),
        "pump test retains the supported target");
  off::runtime::SceneTransitionPump pump;
  std::vector<std::string> pump_events;
  bool removals_precede_handoff = false;
  const off::runtime::SceneTransitionPumpServices pump_services{
      .checked_scenes_resolver = [&](std::string_view target) {
        pump_events.emplace_back("resolve:" + std::string(target));
        return target == "FF-Startup";
      },
      .archive_preparer = [&](std::string_view target) {
        pump_events.emplace_back("prepare:" + std::string(target));
        return target == "FF-Startup";
      },
      .scene_loader_handoff = [&](std::string_view target) {
        removals_precede_handoff = pump_queue.entries().empty();
        pump_events.emplace_back("handoff:" + std::string(target));
      },
  };
  check(pump.consume(pump_queue, pump_services) ==
                off::runtime::SceneTransitionPumpResult::handed_off &&
            !pump_queue.pending() && pump_queue.entries().empty() &&
            pump_queue.targets().empty() && !pump_queue.current_scene().has_value() &&
            removals_precede_handoff &&
            pump_events == std::vector<std::string>{"resolve:FF-Startup",
                                                     "prepare:FF-Startup",
                                                     "handoff:FF-Startup"},
        "pump validates and prepares before retiring entries then handing off");

  off::runtime::SceneTransitionQueue rejected_queue;
  rejected_queue.retain_scene_entry(41U);
  rejected_queue.set_current_scene(41U);
  rejected_queue.request_clear();
  check(rejected_queue.request_target("FF-Startup"),
        "rejection test retains the supported target");
  const off::runtime::SceneTransitionPumpServices rejected_services{
      .checked_scenes_resolver = [](std::string_view) { return false; },
      .archive_preparer = [](std::string_view) { return true; },
      .scene_loader_handoff = [](std::string_view) {},
  };
  check(pump.consume(rejected_queue, rejected_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            rejected_queue.pending() && rejected_queue.entries().size() == 1U &&
            rejected_queue.entries().front().removal_requested &&
            rejected_queue.targets() == std::vector<std::string>{"FF-Startup"} &&
            !rejected_queue.current_scene().has_value(),
        "resolver rejection preserves the pending queue and current scene state");

  off::runtime::SceneTransitionQueue prepare_rejected_queue;
  prepare_rejected_queue.retain_scene_entry(42U);
  prepare_rejected_queue.request_clear();
  check(prepare_rejected_queue.request_target("FF-Startup"),
        "preparer rejection test retains the supported target");
  const off::runtime::SceneTransitionPumpServices prepare_rejected_services{
      .checked_scenes_resolver = [](std::string_view) { return true; },
      .archive_preparer = [](std::string_view) { return false; },
      .scene_loader_handoff = [](std::string_view) { std::abort(); },
  };
  check(pump.consume(prepare_rejected_queue, prepare_rejected_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            prepare_rejected_queue.pending() &&
            prepare_rejected_queue.entries().size() == 1U &&
            prepare_rejected_queue.entries().front().removal_requested &&
            prepare_rejected_queue.targets() ==
                std::vector<std::string>{"FF-Startup"},
        "archive preparation rejection preserves deferred removals and target");

  off::runtime::SceneTransitionQueue reentrant_queue;
  reentrant_queue.request_clear();
  check(reentrant_queue.request_target("FF-Startup"),
        "nonreentrant test retains the supported target");
  bool recursive_call_rejected = false;
  const off::runtime::SceneTransitionPumpServices reentrant_services{
      .checked_scenes_resolver = [&](std::string_view) {
        try {
          static_cast<void>(pump.consume(reentrant_queue, {}));
        } catch (const std::runtime_error&) {
          recursive_call_rejected = true;
        }
        return false;
      },
      .archive_preparer = [](std::string_view) { return true; },
      .scene_loader_handoff = [](std::string_view) {},
  };
  check(pump.consume(reentrant_queue, reentrant_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            recursive_call_rejected && !pump.active() && reentrant_queue.pending(),
        "pump rejects recursive consumption and resets its active guard");

  const auto lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(std::make_shared<const std::uint64_t>(value));
  };
  const auto package = [&] {
    return off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), lease(101U), lease(102U));
  };
  rejected = false;
  try {
    static_cast<void>(off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), {}, lease(102U)));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, "startup transaction rejects a package without complete source leases");
  const auto old_scene = off::runtime::StartupLiveScene::from_factory(71U, lease(71U));
  off::runtime::StartupSceneLoadState startup_state;
  {
    off::runtime::SceneTransitionQueue initial_queue;
    initial_queue.request_clear();
    check(initial_queue.request_target("FF-Startup"),
          "initial transaction retains supported target");
    off::runtime::StartupSceneLoader initial_loader;
    const off::runtime::StartupSceneLoaderServices initial_services{
        .prepare_complete_checked_package = [&](std::string_view target)
            -> std::optional<off::runtime::StartupSceneLoadPackage> {
          return target == "FF-Startup" ? std::optional{package()} : std::nullopt;
        },
        .construct_live_scene = [&](const off::runtime::StartupSceneLoadPackage&)
            -> std::optional<off::runtime::StartupLiveScene> { return old_scene; },
    };
    check(initial_loader.consume(initial_queue, startup_state, initial_services) ==
              off::runtime::StartupSceneLoaderResult::committed &&
              startup_state.current_scene()->identity() == 71U,
          "transaction commits only a factory-produced live scene token");
  }

  off::runtime::SceneTransitionQueue failed_load_queue;
  failed_load_queue.retain_scene_entry(72U);
  failed_load_queue.request_clear();
  check(failed_load_queue.request_target("FF-Startup"),
        "failed transaction retains supported target");
  off::runtime::StartupSceneLoader loader;
  const off::runtime::StartupSceneLoaderServices package_failure{
      .prepare_complete_checked_package = [](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> { return std::nullopt; },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
          -> std::optional<off::runtime::StartupLiveScene> { std::abort(); },
  };
  check(loader.consume(failed_load_queue, startup_state, package_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() && failed_load_queue.entries().size() == 1U &&
            failed_load_queue.targets() == std::vector<std::string>{"FF-Startup"} &&
            startup_state.current_scene()->identity() == 71U,
        "package preparation failure retains the request and prior committed scene");

  const off::runtime::StartupSceneLoaderServices factory_failure{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> { return package(); },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
          -> std::optional<off::runtime::StartupLiveScene> { return std::nullopt; },
  };
  check(loader.consume(failed_load_queue, startup_state, factory_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() && startup_state.current_scene()->identity() == 71U,
        "factory failure cannot retire the prior scene or request");

  const off::runtime::StartupSceneLoaderServices throwing_factory{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> { return package(); },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
          -> std::optional<off::runtime::StartupLiveScene> {
        throw std::runtime_error("factory failed");
      },
  };
  rejected = false;
  try {
    static_cast<void>(loader.consume(failed_load_queue, startup_state, throwing_factory));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected && !loader.active() && failed_load_queue.pending() &&
            startup_state.current_scene()->identity() == 71U,
        "factory exceptions preserve deferred and committed ownership state");

  std::vector<std::string> load_order;
  const off::runtime::StartupSceneLoaderServices committed_loader_services{
      .prepare_complete_checked_package = [&](std::string_view target)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        load_order.emplace_back("prepare:" + std::string(target));
        return package();
      },
      .construct_live_scene = [&](const off::runtime::StartupSceneLoadPackage&)
          -> std::optional<off::runtime::StartupLiveScene> {
        load_order.emplace_back("factory");
        return off::runtime::StartupLiveScene::from_factory(73U, lease(73U));
      },
  };
  check(loader.consume(failed_load_queue, startup_state, committed_loader_services) ==
                off::runtime::StartupSceneLoaderResult::committed &&
            !failed_load_queue.pending() && failed_load_queue.entries().empty() &&
            failed_load_queue.targets().empty() &&
            startup_state.current_scene()->identity() == 73U &&
            load_order == std::vector<std::string>{"prepare:FF-Startup", "factory"},
        "successful transaction prepares then constructs before committing retirement");

  off::runtime::SceneTransitionQueue loader_reentrant_queue;
  loader_reentrant_queue.request_clear();
  check(loader_reentrant_queue.request_target("FF-Startup"),
        "reentrant scene-loader test retains target");
  bool loader_recursive_call_rejected = false;
  const off::runtime::StartupSceneLoaderServices loader_reentrant_services{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        try {
          static_cast<void>(loader.consume(loader_reentrant_queue, startup_state, {}));
        } catch (const std::runtime_error&) {
          loader_recursive_call_rejected = true;
        }
        return std::nullopt;
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
          -> std::optional<off::runtime::StartupLiveScene> { return std::nullopt; },
  };
  check(loader.consume(loader_reentrant_queue, startup_state, loader_reentrant_services) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            loader_recursive_call_rejected && !loader.active() &&
            loader_reentrant_queue.pending(),
        "scene-loader rejects reentrant consumption without consuming the request");

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
