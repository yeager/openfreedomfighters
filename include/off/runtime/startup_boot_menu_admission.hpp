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
  // Opaque first identity resolved by the concrete component reader. It is not
  // a platform action or evidence that the native handler accepts it.
  std::uint64_t reader_identity{};
  std::uint64_t routing_identity{};
};

struct StartupBootMenuObservation {
  // Caller-owned opaque identity observed at an already reconstructed higher
  // layer. This class neither accepts input nor dispatches a native handler.
  std::uint16_t identifier{};
};

struct StartupBootMenuAdmissionServices {
  // EventRegistry: resolve only live caller-provided opaque identities.
  std::function<bool()> event_registry_live;
  std::function<std::optional<std::uint16_t>(std::uint64_t)> resolve_identity;
  // WindowCoordinator: confirms the factory-produced owner remains live, then
  // performs its one-time coordinator initialization using the routing ID.
  std::function<bool(std::uint64_t)> live_window_owner;
  std::function<bool(std::uint64_t, std::uint16_t)> initialize_window;
};

// This is deliberately an admission and observation boundary, not a menu.
// It creates no widgets, does not bind keys/devices, never selects an item or
// requests a scene, and does not establish that a menu is interactive. A
// failure is terminal so callers cannot turn missing retail services into a
// later synthetic start action.
class StartupBootMenuAdmission final {
public:
  void initialize(const StartupBootMenuSource& source,
                  const StartupBootMenuAdmissionServices& services) {
    if (busy_ || failed_ || initialized_) {
      throw std::runtime_error("Startup boot-menu admission is unavailable");
    }
    if (source.runtime_owner == 0U || source.reader_identity == 0U ||
        source.routing_identity == 0U || !services.resolve_identity ||
        !services.event_registry_live ||
        !services.live_window_owner || !services.initialize_window) {
      failed_ = true;
      throw std::runtime_error("Startup boot-menu admission requires live services");
    }

    busy_ = true;
    try {
      if (!services.event_registry_live() ||
          !services.live_window_owner(source.runtime_owner)) {
        throw std::runtime_error("Startup boot-menu owner or registry is not live");
      }
      const auto reader = services.resolve_identity(source.reader_identity);
      const auto routing = services.resolve_identity(source.routing_identity);
      if (!reader || !routing || *reader == 0U || *routing == 0U) {
        throw std::runtime_error("Startup boot-menu event resolution failed");
      }
      if (!services.initialize_window(source.runtime_owner, *routing)) {
        throw std::runtime_error("Startup boot-menu service initialization failed");
      }
      reader_id_ = *reader;
      routing_id_ = *routing;
      initialized_ = true;
    } catch (...) {
      failed_ = true;
      busy_ = false;
      throw;
    }
    busy_ = false;
  }

  // Records an opaque caller-owned observation matching the reader-resolved
  // identity. This has no input, handler, focus, menu, or scene side effect.
  [[nodiscard]] bool observe(const StartupBootMenuObservation event) {
    if (busy_ || failed_) {
      throw std::runtime_error("Startup boot-menu admission is unavailable");
    }
    if (!initialized_ || event.identifier != reader_id_) {
      return false;
    }
    if (observations_ == std::numeric_limits<std::uint32_t>::max()) {
      failed_ = true;
      throw std::runtime_error("Startup boot-menu observation count exhausted");
    }
    ++observations_;
    return true;
  }

  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] std::uint16_t reader_id() const noexcept { return reader_id_; }
  [[nodiscard]] std::uint16_t routing_id() const noexcept { return routing_id_; }
  [[nodiscard]] std::uint32_t observations() const noexcept {
    return observations_;
  }

private:
  bool busy_{};
  bool failed_{};
  bool initialized_{};
  std::uint16_t reader_id_{};
  std::uint16_t routing_id_{};
  std::uint32_t observations_{};
};

}  // namespace off::runtime
