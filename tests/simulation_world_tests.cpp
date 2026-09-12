#include "off/crypto/sha256.hpp"
#include "off/simulation/world.hpp"
#include "off/simulation/world_command_capture.hpp"
#include "off/simulation/replay.hpp"

#include <cstdlib>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

off::simulation::InputSnapshot input(std::uint64_t tick) {
  off::simulation::InputSnapshot result;
  result.tick = tick;
  return result;
}

void write_u64_le(std::vector<std::byte> &bytes, std::size_t offset,
                  std::uint64_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    bytes[offset + index] = static_cast<std::byte>(value & 0xffU);
    value >>= 8U;
  }
}

void refresh_snapshot_checksum(std::vector<std::byte> &bytes) {
  off::crypto::Sha256 hasher;
  hasher.update({bytes.data() + 64, bytes.size() - 64});
  const auto digest = hasher.finish();
  for (std::size_t index = 0; index < digest.size(); ++index)
    bytes[32 + index] = static_cast<std::byte>(digest[index]);
}
} // namespace

int main() {
  using namespace off::simulation;

  {
    SimulationWorld captured;
    WorldCommandCapture capture;
    capture.begin(captured, 3);
    const auto spawn = captured.queue_spawn({{3, 2, 1}, 9});
    const EntityId destroyed{7, 3};
    captured.queue_destroy(destroyed);
    const auto event = captured.queue_event(1, 77, destroyed, {}, -9);
    const auto commands = capture.commands();
    check(commands.size() == 3 && commands[0].issued_after_tick == 0 &&
              commands[0].ordinal == 1 && commands[0].kind == WorldCommandKind::spawn &&
              commands[0].accepted_identifier == spawn && commands[0].spawn.flags == 9 &&
              commands[1].ordinal == 2 && commands[1].kind == WorldCommandKind::destroy &&
              commands[1].destroy == destroyed && commands[2].ordinal == 3 &&
              commands[2].kind == WorldCommandKind::event && commands[2].event.sequence == event,
          "capture accepted cross-kind world commands in one tick-boundary order");
    const auto before_full_capture = captured.state_hash();
    bool capture_rejected = false;
    try { static_cast<void>(captured.queue_spawn({})); } catch (const std::runtime_error &) { capture_rejected = true; }
    check(capture_rejected && captured.state_hash() == before_full_capture && capture.commands().size() == 3,
          "capture capacity rejects before an unrecorded world mutation");
    captured.reset();
    check(!capture.active() && capture.invalidated() && capture.commands().size() == 3 &&
              captured.queue_spawn({}) == 1,
          "world reset terminates capture without recording a discontinuity");
  }
  {
    SimulationWorld replay_world;
    SimulationReplayRecorder recorder;
    recorder.begin(replay_world);
    static_cast<void>(replay_world.queue_spawn({{1, 0, 0}, 1}));
    const auto first_input = input(1);
    static_cast<void>(replay_world.step(first_input));
    recorder.record_completed_step(replay_world, first_input);
    const auto replay = recorder.finish();
    check(replay.initial_snapshot.size() >= 64 && replay.inputs == std::vector{first_input} &&
              replay.checkpoints.size() == 1 && replay.checkpoints[0].completed_tick == 1 &&
              replay.checkpoints[0].state_hash == replay_world.state_hash() && replay.commands.size() == 1,
          "replay recorder retains snapshot, accepted commands, input and post-step checkpoint");
    SimulationWorld replay_target;
    play_replay_atomically(replay_target, replay);
    check(replay_target.state_hash() == replay_world.state_hash(),
          "atomic replay applies accepted commands before its recorded input step");

    const auto envelope = serialize_replay(replay);
    const auto decoded = deserialize_replay(envelope);
    check(decoded.initial_snapshot == replay.initial_snapshot && decoded.inputs == replay.inputs &&
              decoded.commands.size() == replay.commands.size() &&
              decoded.commands[0].kind == replay.commands[0].kind &&
              decoded.commands[0].spawn.position == replay.commands[0].spawn.position &&
              decoded.commands[0].accepted_identifier == replay.commands[0].accepted_identifier &&
              decoded.checkpoints.size() == replay.checkpoints.size() &&
              decoded.checkpoints[0].state_hash == replay.checkpoints[0].state_hash &&
              serialize_replay(decoded) == envelope,
          "versioned replay envelope round-trips canonical project replay data");
    SimulationWorld decoded_target;
    play_replay_atomically(decoded_target, decoded);
    check(decoded_target.state_hash() == replay_world.state_hash(),
          "decoded replay remains eligible for atomic deterministic playback");
    const auto rejects_envelope = [](std::span<const std::byte> bytes) {
      try { static_cast<void>(deserialize_replay(bytes)); } catch (const std::runtime_error &) { return true; }
      return false;
    };
    bool every_truncation_rejected = true;
    for (std::size_t length = 0; length < envelope.size(); ++length) {
      every_truncation_rejected = every_truncation_rejected && rejects_envelope({envelope.data(), length});
    }
    check(every_truncation_rejected, "replay envelope rejects every truncated prefix");
    auto corrupted_envelope = envelope;
    corrupted_envelope[12] ^= std::byte{1};
    check(rejects_envelope(corrupted_envelope),
          "replay envelope rejects payload changes protected by SHA-256");
    auto unsupported_version = envelope;
    unsupported_version[4] = std::byte{2};
    off::crypto::Sha256 version_hasher;
    version_hasher.update({unsupported_version.data(), unsupported_version.size() - 32});
    const auto version_digest = version_hasher.finish();
    for (std::size_t index = 0; index < version_digest.size(); ++index) {
      unsupported_version[unsupported_version.size() - version_digest.size() + index] = static_cast<std::byte>(version_digest[index]);
    }
    check(rejects_envelope(unsupported_version),
          "replay envelope rejects an integrity-valid unsupported schema version");
    SimulationWorld unchanged_target;
    static_cast<void>(unchanged_target.queue_spawn({{9, 9, 9}, 9}));
    static_cast<void>(unchanged_target.step(input(1)));
    const auto target_hash = unchanged_target.state_hash();
    auto divergent = replay;
    divergent.checkpoints[0].state_hash[0] ^= 1U;
    bool playback_rejected = false;
    try { play_replay_atomically(unchanged_target, divergent); } catch (const std::runtime_error &) { playback_rejected = true; }
    check(playback_rejected && unchanged_target.state_hash() == target_hash,
          "late replay checkpoint divergence leaves the destination world unchanged");

    const auto rejects_atomically = [&](SimulationReplay malformed) {
      bool rejected_replay = false;
      try { play_replay_atomically(unchanged_target, malformed); } catch (const std::runtime_error &) { rejected_replay = true; }
      return rejected_replay && unchanged_target.state_hash() == target_hash;
    };
    auto noncanonical_spawn = replay;
    noncanonical_spawn.commands[0].destroy = {2, 3};
    check(rejects_atomically(noncanonical_spawn),
          "replay rejects inactive command payload fields before playback");
    auto terminal_command = replay;
    terminal_command.commands[0].issued_after_tick = terminal_command.inputs.back().tick;
    check(rejects_atomically(terminal_command),
          "replay rejects commands after its final recorded step atomically");
    auto invalid_ordinal = replay;
    invalid_ordinal.commands[0].ordinal = 7;
    check(rejects_atomically(invalid_ordinal),
          "replay validates every command ordinal before playback");
    auto unknown_command = replay;
    unknown_command.commands[0].kind = static_cast<WorldCommandKind>(99);
    check(rejects_atomically(unknown_command),
          "replay rejects unknown command discriminants atomically");
  }

  SimulationWorld world;
  const auto first_request = world.queue_spawn({{1, 2, 3}, 7});
  const auto second_request = world.queue_spawn({{4, 5, 6}, 9});
  const auto event_sequence = world.queue_event(2, 17, {}, {}, -4);
  const auto first_step = world.step(input(1));
  check(first_step.spawned.size() == 2 &&
            first_step.spawned[0].request_id == first_request &&
            first_step.spawned[1].request_id == second_request,
        "activate queued spawns in request order");
  const auto first = first_step.spawned[0].entity;
  const auto second = first_step.spawned[1].entity;
  check(first.index == 0 && second.index == 1 && world.alive(first) &&
            world.entities().size() == 2,
        "assign stable ascending entity slots");

  world.queue_destroy(first);
  world.queue_destroy(first);
  const auto replacement_request = world.queue_spawn({{10, 20, 30}, 11});
  const auto second_step = world.step(input(2));
  check(second_step.destroyed.size() == 1 && !world.alive(first),
        "make duplicate destroys deterministic and idempotent");
  check(second_step.spawned.size() == 1 &&
            second_step.spawned[0].request_id == replacement_request &&
            second_step.spawned[0].entity.index == first.index &&
            second_step.spawned[0].entity.generation == first.generation + 1,
        "reuse the lowest free slot with a new generation");
  check(second_step.events.size() == 1 &&
            second_step.events[0].sequence == event_sequence,
        "deliver tick-addressed events");

  bool rejected = false;
  try {
    static_cast<void>(world.queue_event(2, 1));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "reject current and past event ticks");
  rejected = false;
  try {
    static_cast<void>(world.step(input(4)));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected && world.tick() == 2, "reject skipped input ticks atomically");

  SimulationWorld equal;
  static_cast<void>(equal.queue_spawn({{1, 2, 3}, 7}));
  static_cast<void>(equal.queue_spawn({{4, 5, 6}, 9}));
  static_cast<void>(equal.queue_event(2, 17, {}, {}, -4));
  const auto equal_first = equal.step(input(1));
  equal.queue_destroy(equal_first.spawned[0].entity);
  equal.queue_destroy(equal_first.spawned[0].entity);
  static_cast<void>(equal.queue_spawn({{10, 20, 30}, 11}));
  static_cast<void>(equal.step(input(2)));
  check(world.state_hash() == equal.state_hash(),
        "hash identical canonical world histories equally");
  const auto before_pending = world.state_hash();
  static_cast<void>(world.queue_event(5, 99));
  check(world.state_hash() != before_pending,
        "include pending deterministic work in checkpoints");
  check(off::crypto::to_hex(equal.state_hash()).size() == 64,
        "produce a complete SHA-256 checkpoint");

  SimulationWorld adversarial;
  static_cast<void>(adversarial.queue_spawn({{1, 1, 1}, 1}));
  const auto initial = adversarial.step(input(1)).spawned[0].entity;
  adversarial.queue_destroy(initial);
  static_cast<void>(adversarial.queue_spawn({{2, 2, 2}, 2}));
  const auto replacement = adversarial.step(input(2)).spawned[0].entity;
  adversarial.queue_destroy(initial);
  const auto stale_event =
      adversarial.queue_event(3, 31, initial, replacement, -7);
  const auto live_event =
      adversarial.queue_event(3, 32, replacement, initial, 8);
  const auto adversarial_step = adversarial.step(input(3));
  check(adversarial_step.destroyed.empty() && adversarial.alive(replacement) &&
            !adversarial.alive(initial),
        "never let a stale handle destroy an ABA replacement");
  check(adversarial_step.events.size() == 2 &&
            adversarial_step.events[0].sequence == stale_event &&
            adversarial_step.events[1].sequence == live_event &&
            adversarial_step.events[0].source == initial &&
            adversarial_step.events[1].target == initial,
        "deliver events in request order without rewriting stale identities");

  adversarial.queue_destroy(replacement);
  static_cast<void>(adversarial.queue_spawn({{3, 3, 3}, 3}));
  const auto second_replacement = adversarial.step(input(4)).spawned[0].entity;
  check(second_replacement.index == replacement.index &&
            second_replacement.generation == replacement.generation + 1 &&
            !adversarial.alive(initial) && !adversarial.alive(replacement) &&
            adversarial.alive(second_replacement),
        "apply destroys before spawns and preserve every stale ABA generation");

  const auto atomic_hash = adversarial.state_hash();
  rejected = false;
  try {
    static_cast<void>(adversarial.step(input(6)));
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected && adversarial.state_hash() == atomic_hash,
        "leave canonical state unchanged after a rejected step");

  SimulationWorld original_profile;
  SimulationWorld modern_profile;
  static_cast<void>(original_profile.queue_spawn({{-1, 0, 1}, 0xffffffffU}));
  static_cast<void>(modern_profile.queue_spawn({{-1, 0, 1}, 0xffffffffU}));
  static_cast<void>(original_profile.step(input(1)));
  static_cast<void>(modern_profile.step(input(1)));
  check(original_profile.state_hash() == modern_profile.state_hash(),
        "keep canonical simulation state independent of presentation profile");

  const auto snapshot = world.export_snapshot();
  SimulationWorld restored;
  restored.import_snapshot(snapshot);
  check(restored.state_hash() == world.state_hash() && std::ranges::equal(restored.entities(), world.entities()) &&
            restored.last_input() == world.last_input(),
        "round trip every authoritative world field through the portable snapshot");
  const auto restored_hash = restored.state_hash();
  auto damaged_snapshot = snapshot;
  damaged_snapshot.back() ^= std::byte{1};
  rejected = false;
  try {
    restored.import_snapshot(damaged_snapshot);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected && restored.state_hash() == restored_hash,
        "checksum failure leaves the live world unchanged");
  for (std::size_t length = 0; length < snapshot.size(); ++length) {
    rejected = false;
    try {
      restored.import_snapshot(std::span<const std::byte>(snapshot.data(), length));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    check(rejected && restored.state_hash() == restored_hash,
          "every snapshot truncation is rejected atomically");
  }

  auto terminal_snapshot = SimulationWorld{}.export_snapshot();
  // Payload: four u32 limits followed by tick, sequence, then last-input tick.
  write_u64_le(terminal_snapshot, 64 + 16,
               std::numeric_limits<std::uint64_t>::max());
  write_u64_le(terminal_snapshot, 64 + 16 + 8 + 8,
               std::numeric_limits<std::uint64_t>::max());
  refresh_snapshot_checksum(terminal_snapshot);
  SimulationWorld terminal;
  terminal.import_snapshot(terminal_snapshot);
  const auto terminal_hash = terminal.state_hash();
  rejected = false;
  try {
    static_cast<void>(terminal.step(input(0)));
  } catch (const std::overflow_error &) {
    rejected = true;
  }
  check(rejected && terminal.tick() == std::numeric_limits<std::uint64_t>::max() &&
            terminal.state_hash() == terminal_hash,
        "reject exhausted simulation clocks before tick arithmetic can wrap");

  SimulationWorld bounded({8, 2, 4, 4});
  for (std::size_t index = 0; index < 4; ++index)
    bounded.queue_destroy({std::numeric_limits<std::uint32_t>::max(), 0});
  const auto bounded_hash = bounded.state_hash();
  rejected = false;
  try {
    bounded.queue_destroy({});
  } catch (const std::length_error &) {
    rejected = true;
  }
  check(rejected && bounded.state_hash() == bounded_hash,
        "bound lifecycle commands without mutating state on overflow");

  SimulationWorld entity_bounded({1, 2, 1, 1});
  static_cast<void>(entity_bounded.queue_spawn({{1, 0, 0}, 0}));
  static_cast<void>(entity_bounded.queue_spawn({{2, 0, 0}, 0}));
  const auto entity_bounded_hash = entity_bounded.state_hash();
  rejected = false;
  try {
    static_cast<void>(entity_bounded.step(input(1)));
  } catch (const std::length_error &) {
    rejected = true;
  }
  check(rejected && entity_bounded.tick() == 0 &&
            entity_bounded.entities().empty() &&
            entity_bounded.state_hash() == entity_bounded_hash,
        "reject entity-capacity overflow atomically before deferred mutation");

  SimulationWorld queue_bounded({4, 1, 1, 1});
  static_cast<void>(queue_bounded.queue_spawn({}));
  auto queue_hash = queue_bounded.state_hash();
  rejected = false;
  try {
    static_cast<void>(queue_bounded.queue_spawn({}));
  } catch (const std::length_error &) {
    rejected = true;
  }
  check(rejected && queue_bounded.state_hash() == queue_hash,
        "bound the pending spawn queue atomically");
  static_cast<void>(queue_bounded.queue_event(2, 1));
  queue_hash = queue_bounded.state_hash();
  rejected = false;
  try {
    static_cast<void>(queue_bounded.queue_event(2, 2));
  } catch (const std::length_error &) {
    rejected = true;
  }
  check(rejected && queue_bounded.state_hash() == queue_hash,
        "bound the pending event queue atomically");

  adversarial.reset();
  const SimulationWorld fresh;
  check(adversarial.tick() == 0 && adversarial.entities().empty() &&
            adversarial.last_input().tick == 0 &&
            adversarial.last_input().held == 0 &&
            adversarial.last_input().pressed == 0 &&
            adversarial.last_input().released == 0 &&
            adversarial.last_input().axes == std::array<std::int16_t, 4>{} &&
            adversarial.state_hash() == fresh.state_hash() &&
            adversarial.queue_spawn({{7, 8, 9}, 3}) == 1,
        "reset visible state, pending work, identities, and sequence counters");
  const auto reset_step = adversarial.step(input(1));
  check(reset_step.spawned.size() == 1 &&
            reset_step.spawned[0].entity == EntityId{0, 1},
        "restart entity identity deterministically after reset");
}
