#pragma once

#include "off/simulation/component_store.hpp"

#include <cstdint>
#include <span>

namespace off::simulation {

// Project-owned lifecycle adapter for ComponentStore and SimulationWorld.
//
// ComponentStore intentionally has no opinion about entity liveness so it can
// be independently snapshotted.  This adapter supplies the narrow policy
// needed by a live project simulation: a component may belong only to an
// entity alive at the tick boundary, and every component for an entity that
// the world actually destroys is removed after that same step.  It neither
// assigns component meanings nor changes SimulationWorld queue ordering.
class ComponentWorld final {
public:
  explicit ComponentWorld(WorldLimits world_limits = {},
                          ComponentStoreLimits component_limits = {});

  [[nodiscard]] SimulationWorld &world() noexcept { return world_; }
  [[nodiscard]] const SimulationWorld &world() const noexcept { return world_; }
  [[nodiscard]] const ComponentStore &components() const noexcept {
    return components_;
  }

  void upsert_component(std::uint32_t type, EntityId entity,
                        std::span<const std::byte> payload);
  [[nodiscard]] bool erase_component(std::uint32_t type,
                                     EntityId entity) noexcept;

  // Delegates the world's complete tick unchanged, then removes components
  // owned by each identity that the returned result says was destroyed.
  [[nodiscard]] WorldStepResult step(const InputSnapshot &input);
  void reset() noexcept;

  // Versioned SHA-256 composition of the two project-owned state contracts.
  // It is a checkpoint comparison value, not a retail compatibility format.
  [[nodiscard]] crypto::Sha256Digest state_hash() const;

private:
  SimulationWorld world_;
  ComponentStore components_;
};

} // namespace off::simulation
