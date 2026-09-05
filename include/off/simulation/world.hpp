#pragma once

#include "off/crypto/sha256.hpp"
#include "off/simulation/input.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace off::simulation {

struct EntityId {
  std::uint32_t index{};
  std::uint32_t generation{};
  auto operator<=>(const EntityId &) const = default;
};

struct EntityState {
  EntityId id{};
  std::array<std::int32_t, 3> position{};
  std::uint32_t flags{};
  auto operator<=>(const EntityState &) const = default;
};

struct SpawnState {
  std::array<std::int32_t, 3> position{};
  std::uint32_t flags{};
};

struct SpawnResult {
  std::uint64_t request_id{};
  EntityId entity{};
};

struct SimulationEvent {
  std::uint64_t tick{};
  std::uint64_t sequence{};
  std::uint32_t type{};
  EntityId source{};
  EntityId target{};
  std::int64_t value{};
  auto operator<=>(const SimulationEvent &) const = default;
};

struct WorldStepResult {
  std::uint64_t tick{};
  std::vector<SpawnResult> spawned;
  std::vector<EntityId> destroyed;
  std::vector<SimulationEvent> events;
};

struct WorldLimits {
  std::uint32_t maximum_entities{65'536};
  std::uint32_t maximum_pending_spawns{65'536};
  std::uint32_t maximum_pending_destroys{65'536};
  std::uint32_t maximum_pending_events{262'144};
  auto operator<=>(const WorldLimits &) const = default;
};

class SimulationWorld final {
public:
  explicit SimulationWorld(WorldLimits limits = {});

  [[nodiscard]] std::uint64_t queue_spawn(SpawnState state);
  void queue_destroy(EntityId entity);
  [[nodiscard]] std::uint64_t
  queue_event(std::uint64_t tick, std::uint32_t type, EntityId source = {},
              EntityId target = {}, std::int64_t value = 0);

  // Applies queued destroys, then queued spawns, then delivers events for the
  // snapshot tick. Snapshot ticks must be consecutive and start at one.
  [[nodiscard]] WorldStepResult step(const InputSnapshot &input);
  void reset() noexcept;

  [[nodiscard]] bool alive(EntityId entity) const noexcept;
  [[nodiscard]] std::span<const EntityState> entities() const noexcept {
    return live_entities_;
  }
  [[nodiscard]] const InputSnapshot &last_input() const noexcept {
    return last_input_;
  }
  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }

  // Stable SHA-256 checkpoint over a versioned, explicit little-endian encoding
  // of visible state and pending work. It is a divergence detector, not an
  // authenticity mechanism.
  [[nodiscard]] crypto::Sha256Digest state_hash() const;

private:
  struct Slot {
    std::uint32_t generation{1};
    bool alive{};
    std::array<std::int32_t, 3> position{};
    std::uint32_t flags{};
  };
  struct PendingSpawn {
    std::uint64_t request_id{};
    SpawnState state{};
  };

  void rebuild_live_entities();

  WorldLimits limits_;
  std::uint64_t tick_{};
  std::uint64_t next_sequence_{1};
  std::vector<Slot> slots_;
  std::vector<EntityState> live_entities_;
  std::vector<PendingSpawn> pending_spawns_;
  std::vector<EntityId> pending_destroys_;
  std::vector<SimulationEvent> pending_events_;
  InputSnapshot last_input_{};
};

} // namespace off::simulation
