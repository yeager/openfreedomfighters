#pragma once

#include "off/crypto/sha256.hpp"
#include "off/simulation/world_command_capture.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace off::simulation {
struct ReplayCheckpoint final { std::uint64_t completed_tick{}; crypto::Sha256Digest state_hash{}; };
struct SimulationReplay final {
  std::vector<std::byte> initial_snapshot;
  std::vector<InputSnapshot> inputs;
  std::vector<CapturedWorldCommand> commands;
  std::vector<ReplayCheckpoint> checkpoints;
};
struct SimulationReplayRecorderLimits final { std::size_t maximum_inputs{65'536}; std::size_t maximum_commands{262'144}; std::size_t maximum_checkpoints{65'536}; };
struct ReplayPlaybackLimits final { SnapshotReadLimits snapshot{}; std::size_t maximum_inputs{65'536}; std::size_t maximum_commands{262'144}; std::size_t maximum_checkpoints{65'536}; };
class SimulationReplayRecorder final {
public:
  void begin(SimulationWorld&, SimulationReplayRecorderLimits = {});
  [[nodiscard]] bool active() const noexcept { return capture_.active(); }
  void record_completed_step(const SimulationWorld&, const InputSnapshot&);
  [[nodiscard]] SimulationReplay finish();
  void cancel() noexcept { capture_.end(); world_=nullptr; inputs_.clear(); checkpoints_.clear(); next_tick_=0; }
private:
  WorldCommandCapture capture_;
  const SimulationWorld* world_{};
  SimulationReplayRecorderLimits limits_{};
  std::uint64_t next_tick_{};
  std::vector<InputSnapshot> inputs_;
  std::vector<ReplayCheckpoint> checkpoints_;
};
void play_replay_atomically(SimulationWorld&, const SimulationReplay&, ReplayPlaybackLimits = {});

// Encodes project-authored replay data in the versioned OFF replay envelope.
// The envelope is intentionally unrelated to retail recordings or save files.
// Both directions validate the complete replay grammar before returning data.
[[nodiscard]] std::vector<std::byte> serialize_replay(const SimulationReplay&, ReplayPlaybackLimits = {});
[[nodiscard]] SimulationReplay deserialize_replay(std::span<const std::byte>, ReplayPlaybackLimits = {});
} // namespace off::simulation
