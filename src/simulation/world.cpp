#include "off/simulation/world.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace off::simulation {
namespace {

class StableHash final {
public:
  void byte(std::uint8_t value) {
    const std::array data{static_cast<std::byte>(value)};
    hash_.update(data);
  }
  template <class Integer> void integer(Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(encoded); ++index) {
      byte(static_cast<std::uint8_t>(encoded & 0xffU));
      encoded >>= 8U;
    }
  }
  [[nodiscard]] crypto::Sha256Digest finish() { return hash_.finish(); }

private:
  crypto::Sha256 hash_;
};

void hash_entity(StableHash &hash, EntityId entity) {
  hash.integer(entity.index);
  hash.integer(entity.generation);
}

} // namespace

SimulationWorld::SimulationWorld(WorldLimits limits) : limits_(limits) {
  if (limits.maximum_entities == 0 || limits.maximum_pending_spawns == 0 ||
      limits.maximum_pending_destroys == 0 ||
      limits.maximum_pending_events == 0)
    throw std::invalid_argument("simulation world limits must be positive");
}

std::uint64_t SimulationWorld::queue_spawn(SpawnState state) {
  if (pending_spawns_.size() >= limits_.maximum_pending_spawns)
    throw std::length_error("simulation spawn queue capacity exceeded");
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("simulation sequence exhausted");
  const auto request = next_sequence_++;
  pending_spawns_.push_back({request, state});
  return request;
}

void SimulationWorld::queue_destroy(EntityId entity) {
  if (pending_destroys_.size() >= limits_.maximum_pending_destroys)
    throw std::length_error("simulation destroy queue capacity exceeded");
  pending_destroys_.push_back(entity);
}

std::uint64_t SimulationWorld::queue_event(std::uint64_t tick,
                                           std::uint32_t type, EntityId source,
                                           EntityId target,
                                           std::int64_t value) {
  if (tick <= tick_)
    throw std::invalid_argument("simulation event must target a future tick");
  if (pending_events_.size() >= limits_.maximum_pending_events)
    throw std::length_error("simulation event queue capacity exceeded");
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("simulation sequence exhausted");
  const auto sequence = next_sequence_++;
  pending_events_.push_back({tick, sequence, type, source, target, value});
  return sequence;
}

bool SimulationWorld::alive(EntityId entity) const noexcept {
  return entity.index < slots_.size() && slots_[entity.index].alive &&
         slots_[entity.index].generation == entity.generation;
}

void SimulationWorld::reset() noexcept {
  tick_ = 0;
  next_sequence_ = 1;
  slots_.clear();
  live_entities_.clear();
  pending_spawns_.clear();
  pending_destroys_.clear();
  pending_events_.clear();
  last_input_ = {};
}

WorldStepResult SimulationWorld::step(const InputSnapshot &input) {
  if (input.tick != tick_ + 1)
    throw std::invalid_argument("simulation input tick is not consecutive");
  WorldStepResult result;
  result.tick = input.tick;
  auto reusable = static_cast<std::size_t>(
      std::ranges::count_if(slots_, [](const auto &slot) {
        return !slot.alive &&
               slot.generation != std::numeric_limits<std::uint32_t>::max();
      }));
  std::vector<bool> destroyed_slots(slots_.size(), false);
  for (const auto entity : pending_destroys_) {
    if (alive(entity) && !destroyed_slots[entity.index]) {
      destroyed_slots[entity.index] = true;
      reusable +=
          entity.generation < std::numeric_limits<std::uint32_t>::max() - 1
              ? 1U
              : 0U;
    }
  }
  const auto required_growth =
      pending_spawns_.size() > reusable ? pending_spawns_.size() - reusable : 0;
  if (required_growth > limits_.maximum_entities - slots_.size())
    throw std::length_error("simulation entity capacity exceeded");
  slots_.reserve(slots_.size() + required_growth);
  live_entities_.reserve(slots_.size() + required_growth);
  result.spawned.reserve(pending_spawns_.size());
  result.destroyed.reserve(pending_destroys_.size());
  result.events.reserve(pending_events_.size());

  for (const auto entity : pending_destroys_) {
    if (!alive(entity))
      continue;
    auto &slot = slots_[entity.index];
    slot.alive = false;
    if (slot.generation != std::numeric_limits<std::uint32_t>::max())
      ++slot.generation;
    result.destroyed.push_back(entity);
  }
  pending_destroys_.clear();

  for (const auto &request : pending_spawns_) {
    std::size_t index = 0;
    while (
        index < slots_.size() &&
        (slots_[index].alive ||
         slots_[index].generation == std::numeric_limits<std::uint32_t>::max()))
      ++index;
    if (index == slots_.size()) {
      if (slots_.size() == limits_.maximum_entities)
        throw std::overflow_error("simulation entity capacity exhausted");
      slots_.push_back({});
    }
    auto &slot = slots_[index];
    slot.alive = true;
    slot.position = request.state.position;
    slot.flags = request.state.flags;
    const EntityId entity{static_cast<std::uint32_t>(index), slot.generation};
    result.spawned.push_back({request.request_id, entity});
  }
  pending_spawns_.clear();

  for (const auto &event : pending_events_) {
    if (event.tick == input.tick)
      result.events.push_back(event);
  }
  std::erase_if(pending_events_,
                [&](const auto &event) { return event.tick == input.tick; });
  std::ranges::sort(result.events, {}, &SimulationEvent::sequence);

  tick_ = input.tick;
  last_input_ = input;
  rebuild_live_entities();
  return result;
}

void SimulationWorld::rebuild_live_entities() {
  live_entities_.clear();
  for (std::size_t index = 0; index < slots_.size(); ++index) {
    const auto &slot = slots_[index];
    if (slot.alive) {
      live_entities_.push_back(
          {{static_cast<std::uint32_t>(index), slot.generation},
           slot.position,
           slot.flags});
    }
  }
}

crypto::Sha256Digest SimulationWorld::state_hash() const {
  StableHash hash;
  for (const auto byte :
       std::array<std::uint8_t, 8>{'O', 'F', 'F', 'W', 1, 0, 0, 0})
    hash.byte(byte);
  hash.integer(limits_.maximum_entities);
  hash.integer(limits_.maximum_pending_spawns);
  hash.integer(limits_.maximum_pending_destroys);
  hash.integer(limits_.maximum_pending_events);
  hash.integer(tick_);
  hash.integer(next_sequence_);
  hash.integer(last_input_.tick);
  hash.integer(last_input_.held);
  hash.integer(last_input_.pressed);
  hash.integer(last_input_.released);
  for (const auto axis : last_input_.axes)
    hash.integer(axis);
  hash.integer(static_cast<std::uint64_t>(slots_.size()));
  for (const auto &slot : slots_) {
    hash.integer(slot.generation);
    hash.byte(slot.alive ? 1 : 0);
    for (const auto component : slot.position)
      hash.integer(component);
    hash.integer(slot.flags);
  }
  hash.integer(static_cast<std::uint64_t>(pending_spawns_.size()));
  for (const auto &spawn : pending_spawns_) {
    hash.integer(spawn.request_id);
    for (const auto component : spawn.state.position)
      hash.integer(component);
    hash.integer(spawn.state.flags);
  }
  hash.integer(static_cast<std::uint64_t>(pending_destroys_.size()));
  for (const auto entity : pending_destroys_)
    hash_entity(hash, entity);
  hash.integer(static_cast<std::uint64_t>(pending_events_.size()));
  for (const auto &event : pending_events_) {
    hash.integer(event.tick);
    hash.integer(event.sequence);
    hash.integer(event.type);
    hash_entity(hash, event.source);
    hash_entity(hash, event.target);
    hash.integer(event.value);
  }
  return hash.finish();
}

} // namespace off::simulation
