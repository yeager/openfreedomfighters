#pragma once

#include "off/runtime/startup_boot_scene_construction.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace off::runtime {

class StartupBootMenuAdmission;

// Move-only proof that the component reader has completed for exactly one
// factory-produced BootMenu component. It retains the construction token, so a
// reader-complete component cannot outlive its FF-StartUp scene transaction.
class StartupBootMenuReaderToken final {
public:
  StartupBootMenuReaderToken(const StartupBootMenuReaderToken &) = delete;
  StartupBootMenuReaderToken &
  operator=(const StartupBootMenuReaderToken &) = delete;
  StartupBootMenuReaderToken(StartupBootMenuReaderToken &&) noexcept = default;
  StartupBootMenuReaderToken &
  operator=(StartupBootMenuReaderToken &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return admission_ != nullptr && construction_.valid() && reader_id_ != 0U;
  }
  [[nodiscard]] std::uint64_t owner() const noexcept { return construction_.owner(); }
  [[nodiscard]] std::uint64_t component() const noexcept {
    return construction_.component();
  }

private:
  friend class StartupBootMenuAdmission;
  StartupBootMenuReaderToken(StartupBootControllerToken construction,
                             StartupBootMenuAdmission *admission,
                             std::uint16_t reader_id)
      : construction_(std::move(construction)), admission_(admission),
        reader_id_(reader_id) {}

  StartupBootControllerToken construction_;
  StartupBootMenuAdmission *admission_{};
  std::uint16_t reader_id_{};
};

// Move-only proof that the source-backed BootMenu component completed the
// recovered initialization boundary. Keeping the construction token here
// keeps the checked FF-StartUp scene transaction alive for any later,
// independently recovered boundary. It carries no menu behavior or input
// authority.
class StartupBootMenuInitializationReceipt final {
public:
  StartupBootMenuInitializationReceipt(
      const StartupBootMenuInitializationReceipt &) = delete;
  StartupBootMenuInitializationReceipt &
  operator=(const StartupBootMenuInitializationReceipt &) = delete;
  StartupBootMenuInitializationReceipt(
      StartupBootMenuInitializationReceipt &&) noexcept = default;
  StartupBootMenuInitializationReceipt &
  operator=(StartupBootMenuInitializationReceipt &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return construction_.valid() && reader_id_ != 0U && routing_id_ != 0U;
  }
  [[nodiscard]] std::uint64_t owner() const noexcept {
    return construction_.owner();
  }
  [[nodiscard]] std::uint64_t component() const noexcept {
    return construction_.component();
  }

private:
  friend class StartupBootMenuAdmission;
  StartupBootMenuInitializationReceipt(StartupBootControllerToken construction,
                                       std::uint16_t reader_id,
                                       std::uint16_t routing_id)
      : construction_(std::move(construction)), reader_id_(reader_id),
        routing_id_(routing_id) {}

  StartupBootControllerToken construction_;
  std::uint16_t reader_id_{};
  std::uint16_t routing_id_{};
};

struct StartupBootMenuReaderServices {
  std::function<bool()> event_registry_live;
  // Resolves a caller-owned opaque native registry key. It is not a platform
  // action, a serialized property, or a menu-selection identifier.
  std::function<std::optional<std::uint16_t>(std::uint64_t)> resolve_identity;
  std::function<bool(std::uint64_t)> live_window_owner;
  std::function<bool(std::uint64_t)> live_boot_menu_component;
  // The common reader follows successful resolution of the first identity.
  std::function<bool(std::uint64_t, std::uint64_t)> common_component_reader;
};

struct StartupBootMenuInitializationServices {
  std::function<bool()> event_registry_live;
  std::function<std::optional<std::uint16_t>(std::uint64_t)> resolve_identity;
  std::function<bool(std::uint64_t)> live_window_owner;
  std::function<bool(std::uint64_t)> live_boot_menu_component;
  // Common initialization must precede the second lookup and retained route.
  std::function<bool(std::uint64_t, std::uint64_t)> common_window_initialization;
  std::function<bool(std::uint64_t, std::uint64_t, std::uint16_t)>
      route_retained_object;
};

// A strict two-stage lifecycle boundary for a source-backed BootMenu component.
// It creates no action map or widgets and has no input, focus, selection,
// renderer, or scene-transition behavior.
class StartupBootMenuAdmission final {
public:
  [[nodiscard]] StartupBootMenuReaderToken
  read_component(StartupBootControllerToken construction,
                 std::uint64_t first_lookup_key,
                 const StartupBootMenuReaderServices &services) {
    if (busy_ || failed_ || reader_complete_ || initialized_) {
      throw std::runtime_error("Startup boot-menu reader is unavailable");
    }
    if (!construction.valid() || !construction.source_backed_factory_ ||
        first_lookup_key == 0U ||
        !services.event_registry_live || !services.resolve_identity ||
        !services.live_window_owner || !services.live_boot_menu_component ||
        !services.common_component_reader) {
      failed_ = true;
      throw std::runtime_error("Startup boot-menu reader requires live services");
    }

    busy_ = true;
    try {
      if (!services.event_registry_live() ||
          !services.live_window_owner(construction.owner()) ||
          !services.live_boot_menu_component(construction.component())) {
        throw std::runtime_error("Startup boot-menu reader owner is not live");
      }
      const auto reader = services.resolve_identity(first_lookup_key);
      if (!reader || *reader == 0U) {
        throw std::runtime_error("Startup boot-menu reader identity resolution failed");
      }
      // The concrete component stores this result before its common reader.
      reader_id_ = *reader;
      if (!services.common_component_reader(construction.owner(),
                                            construction.component())) {
        throw std::runtime_error("Startup boot-menu common reader failed");
      }
      reader_complete_ = true;
      busy_ = false;
      return StartupBootMenuReaderToken(std::move(construction), this,
                                        reader_id_);
    } catch (...) {
      reader_id_ = 0U;
      failed_ = true;
      busy_ = false;
      throw;
    }
  }

  [[nodiscard]] StartupBootMenuInitializationReceipt
  initialize_component(StartupBootMenuReaderToken reader_complete,
                       std::uint64_t second_lookup_key,
                       const StartupBootMenuInitializationServices &services) {
    if (busy_ || failed_ || !reader_complete_ || initialized_ ||
        !reader_complete.valid() || reader_complete.admission_ != this ||
        reader_complete.reader_id_ != reader_id_) {
      throw std::runtime_error("Startup boot-menu initialization is unavailable");
    }
    if (second_lookup_key == 0U || !services.event_registry_live ||
        !services.resolve_identity || !services.live_window_owner ||
        !services.live_boot_menu_component ||
        !services.common_window_initialization ||
        !services.route_retained_object) {
      failed_ = true;
      reader_id_ = 0U;
      throw std::runtime_error(
          "Startup boot-menu initialization requires live services");
    }

    busy_ = true;
    try {
      if (!services.event_registry_live() ||
          !services.live_window_owner(reader_complete.owner()) ||
          !services.live_boot_menu_component(reader_complete.component())) {
        throw std::runtime_error("Startup boot-menu initialization owner is not live");
      }
      if (!services.common_window_initialization(reader_complete.owner(),
                                                 reader_complete.component())) {
        throw std::runtime_error("Startup boot-menu common initialization failed");
      }
      const auto routing = services.resolve_identity(second_lookup_key);
      if (!routing || *routing == 0U) {
        throw std::runtime_error(
            "Startup boot-menu routing identity resolution failed");
      }
      if (!services.route_retained_object(reader_complete.owner(),
                                          reader_complete.component(), *routing)) {
        throw std::runtime_error("Startup boot-menu retained routing failed");
      }
      routing_id_ = *routing;
      initialized_ = true;
      busy_ = false;
      return StartupBootMenuInitializationReceipt(
          std::move(reader_complete.construction_), reader_id_, routing_id_);
    } catch (...) {
      reader_id_ = 0U;
      routing_id_ = 0U;
      failed_ = true;
      busy_ = false;
      throw;
    }
  }

  [[nodiscard]] bool reader_complete() const noexcept {
    return reader_complete_ && !failed_;
  }
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] std::uint16_t reader_id() const noexcept {
    return failed_ ? 0U : reader_id_;
  }
  [[nodiscard]] std::uint16_t routing_id() const noexcept {
    return initialized_ ? routing_id_ : 0U;
  }

private:
  bool busy_{};
  bool failed_{};
  bool reader_complete_{};
  bool initialized_{};
  std::uint16_t reader_id_{};
  std::uint16_t routing_id_{};
};

}  // namespace off::runtime
