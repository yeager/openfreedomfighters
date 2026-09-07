#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

namespace off::runtime {

// These values come from a caller that has already constructed the retained
// FF-StartUp object through its source-backed class factory. They are opaque:
// this boundary neither parses them nor gives them input/menu semantics.
struct StartupBootMenuSource {
  std::uint64_t runtime_owner{};
  std::uint64_t action_identity{};
  std::uint64_t routing_identity{};
};

struct StartupBootMenuEvent {
  std::uint16_t identifier{};
};

struct StartupBootMenuAdmissionServices {
  // EventRegistry: resolve only live caller-provided opaque identities.
  std::function<bool()> event_registry_live;
  std::function<std::optional<std::uint16_t>(std::uint64_t)> resolve_event;
  // WindowCoordinator: confirms the factory-produced owner remains live, then
  // performs its one-time coordinator initialization using the routing ID.
  std::function<bool(std::uint64_t)> live_window_owner;
  std::function<bool(std::uint64_t, std::uint16_t)> initialize_window;
  // ActionMap: confirms an existing action map and retains the resolved action.
  std::function<bool()> action_map_live;
  std::function<bool(std::uint16_t)> retain_action;
};

// This is deliberately an admission and observation boundary, not a menu.
// It creates no widgets, does not bind keys/devices, and never selects an item
// or requests a scene. A failure is terminal so callers cannot accidentally
// turn missing retail services into a later synthetic start action.
class StartupBootMenuAdmission final {
public:
  void initialize(const StartupBootMenuSource& source,
                  const StartupBootMenuAdmissionServices& services) {
    if (busy_ || failed_ || interactive_) {
      throw std::runtime_error("Startup boot-menu admission is unavailable");
    }
    if (source.runtime_owner == 0U || source.action_identity == 0U ||
        source.routing_identity == 0U || !services.resolve_event ||
        !services.event_registry_live ||
        !services.live_window_owner || !services.initialize_window ||
        !services.action_map_live || !services.retain_action) {
      failed_ = true;
      throw std::runtime_error("Startup boot-menu admission requires live services");
    }

    busy_ = true;
    try {
      if (!services.event_registry_live() ||
          !services.live_window_owner(source.runtime_owner) ||
          !services.action_map_live()) {
        throw std::runtime_error("Startup boot-menu owner or action map is not live");
      }
      const auto action = services.resolve_event(source.action_identity);
      const auto routing = services.resolve_event(source.routing_identity);
      if (!action || !routing || *action == 0U || *routing == 0U) {
        throw std::runtime_error("Startup boot-menu event resolution failed");
      }
      if (!services.initialize_window(source.runtime_owner, *routing) ||
          !services.retain_action(*action)) {
        throw std::runtime_error("Startup boot-menu service initialization failed");
      }
      action_id_ = *action;
      routing_id_ = *routing;
      interactive_ = true;
    } catch (...) {
      failed_ = true;
      busy_ = false;
      throw;
    }
    busy_ = false;
  }

  // Records that the caller-owned action map delivered the already resolved
  // action. This has no menu, focus, scene, or platform-input side effect.
  [[nodiscard]] bool observe(const StartupBootMenuEvent event) {
    if (busy_ || failed_) {
      throw std::runtime_error("Startup boot-menu admission is unavailable");
    }
    if (!interactive_ || event.identifier != action_id_) {
      return false;
    }
    if (observed_actions_ == std::numeric_limits<std::uint32_t>::max()) {
      failed_ = true;
      throw std::runtime_error("Startup boot-menu action observation count exhausted");
    }
    ++observed_actions_;
    return true;
  }

  [[nodiscard]] bool interactive() const noexcept { return interactive_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] std::uint16_t action_id() const noexcept { return action_id_; }
  [[nodiscard]] std::uint16_t routing_id() const noexcept { return routing_id_; }
  [[nodiscard]] std::uint32_t observed_actions() const noexcept {
    return observed_actions_;
  }

private:
  bool busy_{};
  bool failed_{};
  bool interactive_{};
  std::uint16_t action_id_{};
  std::uint16_t routing_id_{};
  std::uint32_t observed_actions_{};
};

}  // namespace off::runtime
