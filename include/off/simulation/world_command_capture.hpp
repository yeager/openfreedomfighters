#pragma once

#include "off/simulation/world.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace off::simulation {

enum class WorldCommandKind : std::uint8_t { spawn, destroy, event };

struct CapturedWorldCommand final {
  std::uint64_t issued_after_tick{};
  std::uint64_t ordinal{};
  WorldCommandKind kind{};
  SpawnState spawn{};
  EntityId destroy{};
  SimulationEvent event{};
  std::uint64_t accepted_identifier{};
};

// A bounded in-memory recorder for accepted between-tick world mutations.
// It is intentionally not a replay serialization format.
class WorldCommandCapture final {
public:
  WorldCommandCapture() = default;
  ~WorldCommandCapture();
  WorldCommandCapture(const WorldCommandCapture&) = delete;
  WorldCommandCapture& operator=(const WorldCommandCapture&) = delete;

  void begin(SimulationWorld& world, std::size_t maximum_commands);
  void end() noexcept;
  [[nodiscard]] bool active() const noexcept { return world_ != nullptr; }
  [[nodiscard]] bool invalidated() const noexcept { return invalidated_; }
  [[nodiscard]] std::span<const std::byte> initial_snapshot() const noexcept { return initial_snapshot_; }
  [[nodiscard]] std::span<const CapturedWorldCommand> commands() const noexcept { return commands_; }

private:
  friend class SimulationWorld;
  void preflight() const;
  void accepted_spawn(std::uint64_t tick, SpawnState state, std::uint64_t request) noexcept;
  void accepted_destroy(std::uint64_t tick, EntityId entity) noexcept;
  void accepted_event(std::uint64_t tick, SimulationEvent event) noexcept;
  void invalidate_from_world() noexcept;

  SimulationWorld* world_{};
  std::vector<std::byte> initial_snapshot_;
  std::vector<CapturedWorldCommand> commands_;
  std::size_t maximum_commands_{};
  std::uint64_t next_ordinal{1};
  bool invalidated_{};
};

} // namespace off::simulation
