#include "off/platform/startup_lifecycle.hpp"
#include "off/runtime/startloader_load_screen.hpp"
#include "off/runtime/startup_boot_menu_admission.hpp"
#include "off/runtime/startup_boot_scene_construction.hpp"
#include "off/runtime/startup_scene_loader.hpp"
#include "off/runtime/startup_scene_package_source.hpp"
#include "off/runtime/startup_window_hierarchy_snapshot.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <zlib.h>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8U)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto characters = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), characters.begin(), characters.end());
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8U)
    bytes[offset + shift / 8U] =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

std::vector<std::byte> package_gms_fixture() {
  std::vector<std::byte> payload(512);
  const auto write = [&](std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8U)
      payload[offset + shift / 8U] =
          static_cast<std::byte>((value >> shift) & 0xffU);
  };
  write(0, 32);
  write(4, 60);
  write(12, 4);
  write(20, 128);
  write(32, 3);
  write(36, (1U << 24U) | 20U);
  write(40, 7);
  write(44, 84);
  write(48, 0);
  write(52, (1U << 25U) | 84U);
  write(56, 0);
  write(60, 2);
  write(64, 72);
  write(68, 324);
  constexpr char first[] = "first";
  constexpr char second[] = "second";
  std::copy_n(reinterpret_cast<const std::byte *>(first), sizeof(first),
              payload.begin() + 72);
  std::copy_n(reinterpret_cast<const std::byte *>(second), sizeof(second),
              payload.begin() + 324);
  write(80, 32);
  write(84, 384);
  write(88, 420);
  write(96, 0x00100000U);
  write(100, 432);
  write(108, 16);
  write(128, 2);
  write(132, 1);
  write(144, 1);
  write(240, 1);
  write(336, 40);
  write(340, 384);
  write(344, 420);
  write(432, 1);
  write(436, 444);
  for (std::size_t component = 0; component < 9; ++component)
    write(384 + component * 4U,
          std::bit_cast<std::uint32_t>(component % 4U == 0U ? 1.0F : 0.0F));
  write(420, std::bit_cast<std::uint32_t>(10.0F));
  write(424, std::bit_cast<std::uint32_t>(20.0F));
  write(428, std::bit_cast<std::uint32_t>(30.0F));
  write(440, std::bit_cast<std::uint32_t>(2.0F));
  std::vector<std::byte> result;
  append_u32(result, static_cast<std::uint32_t>(payload.size()));
  append_u32(result, static_cast<std::uint32_t>(payload.size() + 9U));
  result.push_back(std::byte{1});
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<std::byte> package_support_fixture() {
  constexpr std::string_view dependency = "fixture.dlc";
  const auto size = static_cast<std::uint32_t>(24U + dependency.size() + 1U);
  std::vector<std::byte> result;
  append_u32(result, 0);
  append_u32(result, 0x80000000U | size);
  append_u32(result, size);
  append_u32(result, 1);
  append_u32(result, 0x46434c44U);
  append_u32(result, size - 16U);
  append_text(result, dependency);
  result.push_back(std::byte{0});
  return result;
}

void write_package_zip(
    const std::filesystem::path &path,
    const std::vector<std::pair<std::string, std::vector<std::byte>>>
        &members) {
  struct Central {
    std::string name;
    std::uint32_t crc, size, offset;
  };
  std::vector<std::byte> bytes;
  std::vector<Central> central;
  for (const auto &[name, contents] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(contents.data()),
                static_cast<uInt>(contents.size())));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, crc);
    append_u32(bytes, static_cast<std::uint32_t>(contents.size()));
    append_u32(bytes, static_cast<std::uint32_t>(contents.size()));
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0);
    append_text(bytes, name);
    bytes.insert(bytes.end(), contents.begin(), contents.end());
    central.push_back(
        {name, crc, static_cast<std::uint32_t>(contents.size()), offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : central) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, entry.size);
    append_u32(bytes, entry.size);
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write startup package fixture");
}

} // namespace

int main() {
  using namespace std::chrono_literals;
  const off::platform::StartupClock::time_point origin{123s};

  const auto package_fixture =
      std::filesystem::current_path() / "off-startup-scene-package-fixture.zip";
  std::error_code package_error;
  std::filesystem::remove(package_fixture, package_error);
  write_package_zip(package_fixture,
                    {{"SCENES/FF-StartUp.GMS", package_gms_fixture()},
                     {"SCENES/FF-StartUp.SUP", package_support_fixture()}});
  bool rejected_package = false;
  try {
    static_cast<void>(off::runtime::StartupScenePackageSource::prepare_checked(
        "other", package_fixture));
  } catch (const std::runtime_error &) {
    rejected_package = true;
  }
  check(rejected_package,
        "package source rejects a non-admitted target before loading");
  auto source_package =
      off::runtime::StartupScenePackageSource::prepare_checked("FF-Startup",
                                                               package_fixture);
  const auto source_package_once =
      std::make_shared<std::optional<off::runtime::StartupSceneLoadPackage>>(
          std::move(source_package));
  off::runtime::SceneTransitionQueue source_package_queue;
  source_package_queue.request_clear();
  check(source_package_queue.request_target("FF-Startup"),
        "source package fixture queues the admitted target");
  off::runtime::StartupSceneLoadState source_package_state;
  off::runtime::StartupSceneLoader source_package_loader;
  const auto source_lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  check(source_package_loader.consume(
            source_package_queue, source_package_state,
            {.prepare_complete_checked_package =
                 [source_package_once](std::string_view) mutable
                 -> std::optional<off::runtime::StartupSceneLoadPackage> {
               return std::move(*source_package_once);
             },
             .construct_live_scene =
                 [&](const off::runtime::StartupSceneLoadPackage &)
                 -> std::optional<off::runtime::StartupLiveScene> {
               return off::runtime::StartupLiveScene::from_factory(
                   9U, source_lease(9U));
             }}) == off::runtime::StartupSceneLoaderResult::committed &&
            source_package_state.current_scene()->identity() == 9U,
        "package source supplies a durable package compatible with the loader "
        "transaction");
  write_package_zip(package_fixture,
                    {{"SCENES/FF-StartUp.GMS", package_gms_fixture()},
                     {"SCENES/duplicate.GMS", package_gms_fixture()},
                     {"SCENES/FF-StartUp.SUP", package_support_fixture()}});
  rejected_package = false;
  try {
    static_cast<void>(off::runtime::StartupScenePackageSource::prepare_checked(
        "FF-Startup", package_fixture));
  } catch (const std::runtime_error &) {
    rejected_package = true;
  }
  check(rejected_package,
        "package source rejects a duplicate selected GMS family");
  std::filesystem::remove(package_fixture, package_error);

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
    static_cast<void>(
        off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
            "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", false));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "LoadScreen source requires a caller-proven exact wrapper");
  rejected = false;
  try {
    static_cast<void>(
        off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
            "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", true));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "LoadScreen source rejects a non-supported target");

  const auto source =
      off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
          "ZWINGROUP_LoadScreen", 0.0F, "FF-Startup", true);
  off::runtime::SceneTransitionQueue transitions;
  transitions.retain_scene_entry(11U);
  transitions.retain_scene_entry(12U);
  transitions.set_current_scene(11U);
  off::runtime::LoadScreenTransition load_screen(source);
  bool setup_missing = false;
  try {
    static_cast<void>(load_screen.ordinary_update(transitions));
  } catch (const std::runtime_error &) {
    setup_missing = true;
  }
  check(setup_missing && load_screen.one_time_setup_pending() &&
            load_screen.update_count() == 0U,
        "a missing setup service preserves LoadScreen state");
  std::uint32_t setup_calls{};
  const auto setup = [&] { ++setup_calls; };
  check(
      !load_screen.ordinary_update(transitions, setup) &&
          !load_screen.ordinary_update(transitions, setup) &&
          setup_calls == 1U && transitions.targets().empty(),
      "LoadScreen setup runs once and the first two updates retain no target");
  check(load_screen.ordinary_update(transitions, setup) && setup_calls == 1U &&
            transitions.clear_requests() == 1U && transitions.pending() &&
            !transitions.current_scene().has_value() &&
            transitions.targets().size() == 1U &&
            transitions.targets().front() == "FF-Startup" &&
            load_screen.retained_target().empty(),
        "third LoadScreen update clears then queues its retained target");
  check(transitions.entries().size() == 2U &&
            transitions.entries()[0].removal_requested &&
            transitions.entries()[1].removal_requested,
        "clear request marks retained entries without consuming them");
  check(!load_screen.ordinary_update(transitions, setup) &&
            transitions.clear_requests() == 2U &&
            transitions.targets().size() == 1U,
        "post-request updates retain native empty-target behavior without "
        "loading a scene");

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
  off::runtime::StartupSceneLoadState pump_state;
  const auto pump_lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  const off::runtime::SceneTransitionPumpServices pump_services{
      .startup_loader = {
          .prepare_complete_checked_package =
              [&](std::string_view target)
              -> std::optional<off::runtime::StartupSceneLoadPackage> {
            pump_events.emplace_back("prepare:" + std::string(target));
            return off::runtime::StartupSceneLoadPackage::complete(
                target, pump_lease(31U), pump_lease(32U), pump_lease(33U));
          },
          .construct_live_scene =
              [&](const off::runtime::StartupSceneLoadPackage&)
              -> std::optional<off::runtime::StartupLiveScene> {
            // Regression: the old notification pump had already consumed this
            // request before invoking its handoff. Construction must instead
            // observe the still-owned pending request and marked removals.
            check(pump_queue.pending() && pump_queue.entries().size() == 2U &&
                      pump_queue.targets() ==
                          std::vector<std::string>{"FF-Startup"},
                  "manager pump retains request until the live candidate exists");
            pump_events.emplace_back("factory");
            return off::runtime::StartupLiveScene::from_factory(
                34U, pump_lease(34U));
          },
      },
  };
  check(pump.consume(pump_queue, pump_state, pump_services) ==
                off::runtime::SceneTransitionPumpResult::committed &&
            !pump_queue.pending() && pump_queue.entries().empty() &&
            pump_queue.targets().empty() &&
            !pump_queue.current_scene().has_value() &&
            pump_state.current_scene()->identity() == 34U &&
            pump_events == std::vector<std::string>{"prepare:FF-Startup",
                                                    "factory"},
        "manager pump keeps its request through construction then commits once");

  off::runtime::SceneTransitionQueue rejected_queue;
  rejected_queue.retain_scene_entry(41U);
  rejected_queue.set_current_scene(41U);
  rejected_queue.request_clear();
  check(rejected_queue.request_target("FF-Startup"),
        "rejection test retains the supported target");
  const off::runtime::SceneTransitionPumpServices rejected_services{
      .startup_loader = {
          .prepare_complete_checked_package = [](std::string_view)
              -> std::optional<off::runtime::StartupSceneLoadPackage> {
            return std::nullopt;
          },
          .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
              -> std::optional<off::runtime::StartupLiveScene> {
            return std::nullopt;
          },
      },
  };
  check(
      pump.consume(rejected_queue, pump_state, rejected_services) ==
              off::runtime::SceneTransitionPumpResult::rejected &&
          rejected_queue.pending() && rejected_queue.entries().size() == 1U &&
          rejected_queue.entries().front().removal_requested &&
          rejected_queue.targets() == std::vector<std::string>{"FF-Startup"} &&
          !rejected_queue.current_scene().has_value() &&
          pump_state.current_scene()->identity() == 34U,
      "manager-pump rejection preserves its pending request and prior scene");

  off::runtime::SceneTransitionQueue multiple_targets_queue;
  multiple_targets_queue.request_clear();
  check(multiple_targets_queue.request_target("FF-Startup") &&
            multiple_targets_queue.request_target("FF-Startup"),
        "multiple-target test retains both requests");
  check(pump.consume(multiple_targets_queue, pump_state, pump_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            multiple_targets_queue.pending() &&
            multiple_targets_queue.targets().size() == 2U &&
            pump_state.current_scene()->identity() == 34U,
        "manager pump leaves unsupported target ordering untouched");

  off::runtime::SceneTransitionQueue throwing_queue;
  throwing_queue.retain_scene_entry(43U);
  throwing_queue.request_clear();
  check(throwing_queue.request_target("FF-Startup"),
        "throwing manager-pump test retains target");
  bool pump_threw = false;
  try {
    static_cast<void>(pump.consume(
        throwing_queue, pump_state,
        {.startup_loader = {
             .prepare_complete_checked_package = [](std::string_view)
                 -> std::optional<off::runtime::StartupSceneLoadPackage> {
               throw std::runtime_error("package failed");
             },
             .construct_live_scene =
                 [](const off::runtime::StartupSceneLoadPackage&)
                 -> std::optional<off::runtime::StartupLiveScene> {
               return std::nullopt;
             },
         }}));
  } catch (const std::runtime_error&) {
    pump_threw = true;
  }
  check(pump_threw && !pump.active() && throwing_queue.pending() &&
            throwing_queue.entries().size() == 1U &&
            throwing_queue.targets() ==
                std::vector<std::string>{"FF-Startup"} &&
            pump_state.current_scene()->identity() == 34U,
        "manager-pump exceptions preserve deferred and committed state");

  off::runtime::SceneTransitionQueue reentrant_queue;
  reentrant_queue.request_clear();
  check(reentrant_queue.request_target("FF-Startup"),
        "nonreentrant test retains the supported target");
  bool recursive_call_rejected = false;
  const off::runtime::SceneTransitionPumpServices reentrant_services{
      .startup_loader = {
          .prepare_complete_checked_package =
          [&](std::string_view) -> std::optional<off::runtime::StartupSceneLoadPackage> {
            try {
              static_cast<void>(pump.consume(reentrant_queue, pump_state, {}));
            } catch (const std::runtime_error &) {
              recursive_call_rejected = true;
            }
            return std::nullopt;
          },
          .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage&)
              -> std::optional<off::runtime::StartupLiveScene> {
            return std::nullopt;
          },
      },
  };
  check(pump.consume(reentrant_queue, pump_state, reentrant_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            recursive_call_rejected && !pump.active() &&
            reentrant_queue.pending(),
        "pump rejects recursive consumption and resets its active guard");

  const auto lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  const auto package = [&] {
    return off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), lease(101U), lease(102U));
  };
  rejected = false;
  try {
    static_cast<void>(off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), {}, lease(102U)));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "startup transaction rejects a package without complete source leases");
  const auto old_scene =
      off::runtime::StartupLiveScene::from_factory(71U, lease(71U));
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
          return target == "FF-Startup" ? std::optional{package()}
                                        : std::nullopt;
        },
        .construct_live_scene =
            [&](const off::runtime::StartupSceneLoadPackage &)
            -> std::optional<off::runtime::StartupLiveScene> {
          return old_scene;
        },
    };
    check(initial_loader.consume(initial_queue, startup_state,
                                 initial_services) ==
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
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return std::nullopt;
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> { std::abort(); },
  };
  check(loader.consume(failed_load_queue, startup_state, package_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() &&
            failed_load_queue.entries().size() == 1U &&
            failed_load_queue.targets() ==
                std::vector<std::string>{"FF-Startup"} &&
            startup_state.current_scene()->identity() == 71U,
        "package preparation failure retains the request and prior committed "
        "scene");

  const off::runtime::StartupSceneLoaderServices factory_failure{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return package();
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        return std::nullopt;
      },
  };
  check(loader.consume(failed_load_queue, startup_state, factory_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() &&
            startup_state.current_scene()->identity() == 71U,
        "factory failure cannot retire the prior scene or request");

  const off::runtime::StartupSceneLoaderServices throwing_factory{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return package();
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        throw std::runtime_error("factory failed");
      },
  };
  rejected = false;
  try {
    static_cast<void>(
        loader.consume(failed_load_queue, startup_state, throwing_factory));
  } catch (const std::runtime_error &) {
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
      .construct_live_scene = [&](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        load_order.emplace_back("factory");
        return off::runtime::StartupLiveScene::from_factory(73U, lease(73U));
      },
  };
  check(loader.consume(failed_load_queue, startup_state,
                       committed_loader_services) ==
                off::runtime::StartupSceneLoaderResult::committed &&
            !failed_load_queue.pending() &&
            failed_load_queue.entries().empty() &&
            failed_load_queue.targets().empty() &&
            startup_state.current_scene()->identity() == 73U &&
            load_order ==
                std::vector<std::string>{"prepare:FF-Startup", "factory"},
        "successful transaction prepares then constructs before committing "
        "retirement");

  off::runtime::SceneTransitionQueue loader_reentrant_queue;
  loader_reentrant_queue.request_clear();
  check(loader_reentrant_queue.request_target("FF-Startup"),
        "reentrant scene-loader test retains target");
  bool loader_recursive_call_rejected = false;
  const off::runtime::StartupSceneLoaderServices loader_reentrant_services{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        try {
          static_cast<void>(
              loader.consume(loader_reentrant_queue, startup_state, {}));
        } catch (const std::runtime_error &) {
          loader_recursive_call_rejected = true;
        }
        return std::nullopt;
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        return std::nullopt;
      },
  };
  check(loader.consume(loader_reentrant_queue, startup_state,
                       loader_reentrant_services) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            loader_recursive_call_rejected && !loader.active() &&
            loader_reentrant_queue.pending(),
        "scene-loader rejects reentrant consumption without consuming the "
        "request");

  static_assert(
      !std::is_copy_constructible_v<off::runtime::StartupBootControllerToken>);
  const auto boot_package =
      std::make_shared<const off::runtime::StartupSceneLoadPackage>(package());
  const auto boot_scene = off::runtime::StartupBootSceneLease::live(lease(81U));
  const off::runtime::StartupBootSceneDirectoryProof boot_directory{
      .complete_directory_mapping = true,
      .canonical_ordinary_window_source = true,
      .component_identifier = "ZWINDOW_BootMenu",
      .component_parameter = 1.0F,
  };
  std::uint64_t attached_owner{};
  float attached_parameter{};
  const off::runtime::StartupBootSceneConstructionServices
      boot_construction_services{
          .registry_live = [] { return true; },
          .allocate_ordinary_window = [] { return 81U; },
          .canonical_live_window_owner =
              [](std::uint64_t owner) { return owner == 81U; },
          .attach_boot_menu_component =
              [&](std::uint64_t owner, float parameter) {
                attached_owner = owner;
                attached_parameter = parameter;
                return 82U;
              },
          .live_boot_menu_component =
              [](std::uint64_t component) { return component == 82U; },
      };
  off::runtime::StartupBootSceneConstruction boot_construction;
  auto boot_token = boot_construction.construct(
      boot_package, boot_scene, boot_directory, 9U, boot_construction_services);
  check(boot_token.valid() && boot_token.owner() == 81U &&
            boot_token.component() == 82U &&
            boot_token.factory_generation() == 9U && attached_owner == 81U &&
            attached_parameter == 1.0F,
        "boot construction retains only a checked factory-produced owner and "
        "component");

  rejected = false;
  try {
    auto invalid_boot_directory = boot_directory;
    invalid_boot_directory.component_parameter = 0.0F;
    static_cast<void>(boot_construction.construct(boot_package, boot_scene,
                                                  invalid_boot_directory, 9U,
                                                  boot_construction_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "boot construction rejects a non-exact component attachment "
                  "before allocation");

  rejected = false;
  try {
    auto missing_component_services = boot_construction_services;
    missing_component_services.live_boot_menu_component = [](std::uint64_t) {
      return false;
    };
    static_cast<void>(boot_construction.construct(boot_package, boot_scene,
                                                  boot_directory, 9U,
                                                  missing_component_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(
      rejected,
      "boot construction fails closed when the factory component is not live");

  static_assert(!std::is_copy_constructible_v<
                off::runtime::StartupWindowHierarchySnapshot>);
  const std::array<std::size_t, 4> hierarchy_root_children{11U, 12U, 13U, 14U};
  const std::array<std::size_t, 1> hierarchy_first_container_children{15U};
  const std::array<off::runtime::StartupWindowHierarchySourceNode, 6>
      hierarchy_source{{
          {10U, std::nullopt, hierarchy_root_children},
          {11U, 10U, hierarchy_first_container_children},
          {12U, 10U, {}}, {13U, 10U, {}}, {14U, 10U, {}}, {15U, 11U, {}},
      }};
  const off::runtime::StartupWindowHierarchySourceScope hierarchy_scope{
      true, 10U, hierarchy_source};
  std::unordered_map<std::size_t, off::runtime::StartupFactoryProvenWindowNode>
      live_hierarchy{{
          {10U, {100U, 10U, std::nullopt,
                 off::runtime::StartupWindowNodeFamily::container, 103U,
                 std::nullopt}},
          {11U, {101U, 11U, 100U,
                 off::runtime::StartupWindowNodeFamily::container, 105U, 102U}},
          {12U, {102U, 12U, 100U,
                 off::runtime::StartupWindowNodeFamily::leaf, std::nullopt,
                 104U}},
          {13U, {103U, 13U, 100U,
                 off::runtime::StartupWindowNodeFamily::container, std::nullopt,
                 101U}},
          {14U, {104U, 14U, 100U,
                 off::runtime::StartupWindowNodeFamily::leaf, std::nullopt,
                 std::nullopt}},
          {15U, {105U, 15U, 101U,
                 off::runtime::StartupWindowNodeFamily::leaf, std::nullopt,
                 std::nullopt}},
      }};
  std::uint32_t guard_begins{};
  std::uint32_t guard_ends{};
  std::uint64_t observed_guard{};
  std::uint64_t hierarchy_epoch = 71U;
  const off::runtime::StartupWindowHierarchySnapshotServices hierarchy_services{
      .begin_read_guard = [&]() -> std::optional<std::uint64_t> {
        ++guard_begins;
        return 91U;
      },
      .end_read_guard = [&](std::uint64_t guard) {
        ++guard_ends;
        observed_guard = guard;
      },
      .hierarchy_epoch = [&] { return hierarchy_epoch; },
      .factory_generation_live = [](std::uint64_t generation) {
        return generation == 9U;
      },
      .read_factory_proven_node = [&](std::size_t source)
          -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
        const auto found = live_hierarchy.find(source);
        return found == live_hierarchy.end()
                   ? std::nullopt
                   : std::optional<off::runtime::StartupFactoryProvenWindowNode>{
                         found->second};
      },
  };
  const auto hierarchy_lease =
      off::runtime::StartupWindowHierarchyLease::live(lease(100U));
  off::runtime::StartupWindowHierarchyFactory hierarchy_factory;
  auto hierarchy_snapshot = hierarchy_factory.capture(
      hierarchy_lease, 9U, hierarchy_scope, hierarchy_services);
  check(hierarchy_snapshot.valid() && hierarchy_snapshot.bound_to(9U, 71U) &&
            hierarchy_snapshot.root_identity() == 100U &&
            hierarchy_snapshot.construction_preorder() ==
                std::vector<std::uint64_t>{100U, 103U, 101U, 105U, 102U, 104U} &&
            guard_begins == 1U && guard_ends == 1U && observed_guard == 91U,
        "factory-proven startup hierarchy snapshots reverse containers, retain "
        "leaves and preorder links");

  rejected = false;
  try {
    auto source_only_services = hierarchy_services;
    source_only_services.read_factory_proven_node =
        [](std::size_t)
        -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
      return std::nullopt;
    };
    static_cast<void>(hierarchy_factory.capture(hierarchy_lease, 9U,
                                                hierarchy_scope, source_only_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "startup hierarchy rejects source-only nodes without factory provenance");

  rejected = false;
  try {
    auto cyclic = live_hierarchy;
    cyclic.at(14U).next_sibling_identity = 103U;
    auto cycle_services = hierarchy_services;
    cycle_services.read_factory_proven_node = [&](std::size_t source)
        -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
      const auto found = cyclic.find(source);
      return found == cyclic.end()
                 ? std::nullopt
                 : std::optional<off::runtime::StartupFactoryProvenWindowNode>{
                       found->second};
    };
    static_cast<void>(hierarchy_factory.capture(hierarchy_lease, 9U,
                                                hierarchy_scope, cycle_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "startup hierarchy rejects a cyclic or policy-invalid live sibling chain");

  rejected = false;
  try {
    const std::array<off::runtime::StartupWindowHierarchySourceNode, 5>
        partial_source{{
            {10U, std::nullopt, hierarchy_root_children},
            {11U, 10U, hierarchy_first_container_children}, {12U, 10U, {}},
            {13U, 10U, {}}, {14U, 10U, {}},
        }};
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, {true, 10U, partial_source}, hierarchy_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup hierarchy rejects a partial source scope");

  rejected = false;
  try {
    std::uint32_t reads{};
    auto changed_epoch_services = hierarchy_services;
    changed_epoch_services.hierarchy_epoch = [&] {
      return reads++ == 0U ? 71U : 72U;
    };
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, hierarchy_scope, changed_epoch_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "startup hierarchy invalidates a snapshot when the guarded epoch changes");

  static_assert(
      !std::is_copy_constructible_v<off::runtime::StartupBootMenuReaderToken>);
  off::runtime::StartupBootMenuAdmission boot_menu;
  std::uint32_t resolve_calls{};
  std::vector<std::string> lifecycle_order;
  const off::runtime::StartupBootMenuReaderServices reader_services{
      .event_registry_live = [] { return true; },
      .resolve_identity =
          [&](std::uint64_t identity) -> std::optional<std::uint16_t> {
        ++resolve_calls;
        return identity == 101U ? std::optional<std::uint16_t>{31U}
                                : std::nullopt;
      },
      .live_window_owner = [](std::uint64_t owner) { return owner == 81U; },
      .live_boot_menu_component =
          [](std::uint64_t component) { return component == 82U; },
      .common_component_reader = [&](std::uint64_t owner, std::uint64_t component) {
        lifecycle_order.push_back("common-reader");
        return owner == 81U && component == 82U;
      },
  };
  auto reader_token =
      boot_menu.read_component(std::move(boot_token), 101U, reader_services);
  check(boot_menu.reader_complete() && !boot_menu.initialized() &&
            !boot_menu.failed() && reader_token.valid() && resolve_calls == 1U &&
            boot_menu.reader_id() == 31U && lifecycle_order.size() == 1U &&
            lifecycle_order[0] == "common-reader",
        "boot-menu reader resolves only its first opaque identity before the "
        "common reader");

  std::uint64_t initialized_owner{};
  std::uint64_t initialized_component{};
  std::uint16_t initialized_route{};
  const off::runtime::StartupBootMenuInitializationServices
      initialization_services{
          .event_registry_live = [] { return true; },
          .resolve_identity =
              [&](std::uint64_t identity) -> std::optional<std::uint16_t> {
            ++resolve_calls;
            lifecycle_order.push_back("second-lookup");
            return identity == 102U ? std::optional<std::uint16_t>{32U}
                                    : std::nullopt;
          },
          .live_window_owner = [](std::uint64_t owner) { return owner == 81U; },
          .live_boot_menu_component =
              [](std::uint64_t component) { return component == 82U; },
          .common_window_initialization =
              [&](std::uint64_t owner, std::uint64_t component) {
                lifecycle_order.push_back("common-initialization");
                return owner == 81U && component == 82U;
              },
          .route_retained_object =
              [&](std::uint64_t owner, std::uint64_t component,
                  std::uint16_t route) {
                lifecycle_order.push_back("retained-route");
                initialized_owner = owner;
                initialized_component = component;
                initialized_route = route;
                return true;
              },
      };
  boot_menu.initialize_component(std::move(reader_token), 102U,
                                 initialization_services);
  check(boot_menu.initialized() && !boot_menu.failed() && resolve_calls == 2U &&
            initialized_owner == 81U && initialized_component == 82U &&
            initialized_route == 32U && boot_menu.reader_id() == 31U &&
            boot_menu.routing_id() == 32U && lifecycle_order.size() == 4U &&
            lifecycle_order[1] == "common-initialization" &&
            lifecycle_order[2] == "second-lookup" &&
            lifecycle_order[3] == "retained-route",
        "boot-menu initialization runs common initialization, second opaque "
        "lookup, and retained routing in order");

  off::runtime::StartupBootMenuAdmission missing_registry;
  auto missing_token = boot_construction.construct(
      boot_package, boot_scene, boot_directory, 10U, boot_construction_services);
  auto unavailable_registry_services = reader_services;
  unavailable_registry_services.event_registry_live = [] { return false; };
  rejected = false;
  try {
    static_cast<void>(missing_registry.read_component(
        std::move(missing_token), 101U, unavailable_registry_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && missing_registry.failed() &&
            !missing_registry.reader_complete() &&
            !missing_registry.initialized() && missing_registry.reader_id() == 0U,
        "boot-menu reader fails closed without exposing an identity when the "
        "registry is absent");

  off::runtime::StartupBootMenuAdmission routing_failure;
  auto routing_token = boot_construction.construct(
      boot_package, boot_scene, boot_directory, 11U, boot_construction_services);
  auto routing_reader = routing_failure.read_component(
      std::move(routing_token), 101U, reader_services);
  auto failed_routing_services = initialization_services;
  failed_routing_services.route_retained_object =
      [](std::uint64_t, std::uint64_t, std::uint16_t) { return false; };
  rejected = false;
  try {
    routing_failure.initialize_component(std::move(routing_reader), 102U,
                                         failed_routing_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && routing_failure.failed() && !routing_failure.initialized() &&
            routing_failure.reader_id() == 0U && routing_failure.routing_id() == 0U,
        "boot-menu initialization does not expose partial state when retained "
        "routing fails");

  std::cout << "startup lifecycle tests passed\n";
}
