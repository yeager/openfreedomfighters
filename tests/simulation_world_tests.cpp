#include "off/crypto/sha256.hpp"
#include "off/simulation/world.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

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
} // namespace

int main() {
  using namespace off::simulation;

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
