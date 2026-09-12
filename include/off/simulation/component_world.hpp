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

  [[nodiscard]] const SimulationWorld &world() const noexcept { return world_; }
  [[nodiscard]] const ComponentStore &components() const noexcept {
    return components_;
  }

  void upsert_component(std::uint32_t type, EntityId entity,
                        std::span<const std::byte> payload);
  [[nodiscard]] bool erase_component(std::uint32_t type,
                                     EntityId entity) noexcept;

  // These are the only live-world mutations exposed by the adapter.  Keeping
  // reset and snapshot replacement private prevents a caller from replacing
  // entity lifetimes while leaving project-owned components behind.
  [[nodiscard]] std::uint64_t queue_spawn(SpawnState state);
  void queue_destroy(EntityId entity);
  [[nodiscard]] std::uint64_t
  queue_event(std::uint64_t tick, std::uint32_t type, EntityId source = {},
              EntityId target = {}, std::int64_t value = 0);

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
