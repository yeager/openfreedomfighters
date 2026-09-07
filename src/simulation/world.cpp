#include "off/simulation/world.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

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

class SnapshotWriter final {
public:
  template <class Integer> void integer(Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(encoded); ++index) {
      bytes_.push_back(static_cast<std::byte>(encoded & 0xffU));
      encoded >>= 8U;
    }
  }
  void byte(std::uint8_t value) { bytes_.push_back(static_cast<std::byte>(value)); }
  [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }
private:
  std::vector<std::byte> bytes_;
};

class SnapshotReader final {
public:
  explicit SnapshotReader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  template <class Integer> Integer integer() {
    using Unsigned = std::make_unsigned_t<Integer>;
    if (bytes_.size() - position_ < sizeof(Unsigned)) throw std::invalid_argument("truncated simulation snapshot");
    Unsigned result{};
    for (std::size_t index = 0; index < sizeof(Unsigned); ++index)
      result |= static_cast<Unsigned>(std::to_integer<std::uint8_t>(bytes_[position_++])) << (index * 8U);
    return static_cast<Integer>(result);
  }
  [[nodiscard]] bool exhausted() const noexcept { return position_ == bytes_.size(); }
private:
  std::span<const std::byte> bytes_;
  std::size_t position_{};
};

void snapshot_input(SnapshotWriter &writer, const InputSnapshot &input) {
  writer.integer(input.tick); writer.integer(input.held); writer.integer(input.pressed); writer.integer(input.released);
  for (const auto axis : input.axes) writer.integer(axis);
}
InputSnapshot read_snapshot_input(SnapshotReader &reader) {
  InputSnapshot input{.tick=reader.integer<std::uint64_t>(), .held=reader.integer<std::uint64_t>(),
                      .pressed=reader.integer<std::uint64_t>(), .released=reader.integer<std::uint64_t>()};
  for (auto &axis : input.axes) axis=reader.integer<std::int16_t>();
  return input;
}
void snapshot_entity(SnapshotWriter &writer, EntityId entity) { writer.integer(entity.index); writer.integer(entity.generation); }
EntityId read_snapshot_entity(SnapshotReader &reader) { return {reader.integer<std::uint32_t>(),reader.integer<std::uint32_t>()}; }

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

std::vector<std::byte> SimulationWorld::export_snapshot() const {
  SnapshotWriter payload;
  payload.integer(limits_.maximum_entities); payload.integer(limits_.maximum_pending_spawns);
  payload.integer(limits_.maximum_pending_destroys); payload.integer(limits_.maximum_pending_events);
  payload.integer(tick_); payload.integer(next_sequence_); snapshot_input(payload,last_input_);
  payload.integer(static_cast<std::uint32_t>(slots_.size()));
  for(const auto &slot:slots_) { payload.integer(slot.generation); payload.byte(slot.alive?1:0); for(auto value:slot.position) payload.integer(value); payload.integer(slot.flags); }
  payload.integer(static_cast<std::uint32_t>(pending_spawns_.size()));
  for(const auto &spawn:pending_spawns_) { payload.integer(spawn.request_id); for(auto value:spawn.state.position) payload.integer(value); payload.integer(spawn.state.flags); }
  payload.integer(static_cast<std::uint32_t>(pending_destroys_.size())); for(auto entity:pending_destroys_) snapshot_entity(payload,entity);
  payload.integer(static_cast<std::uint32_t>(pending_events_.size()));
  for(const auto &event:pending_events_) { payload.integer(event.tick); payload.integer(event.sequence); payload.integer(event.type); snapshot_entity(payload,event.source); snapshot_entity(payload,event.target); payload.integer(event.value); }
  auto body=std::move(payload).take();
  crypto::Sha256 checksum; checksum.update(body); const auto hash=checksum.finish();
  SnapshotWriter output;
  for(const auto value:std::array<std::uint8_t,8>{'O','F','F','S','I','M',0,0}) output.byte(value);
  output.integer<std::uint32_t>(1); output.integer<std::uint32_t>(1); output.integer<std::uint32_t>(0x01020304U); output.integer<std::uint32_t>(64);
  output.integer(static_cast<std::uint64_t>(body.size())); for(auto byte:hash) output.byte(byte);
  auto result=std::move(output).take(); result.insert(result.end(),body.begin(),body.end()); return result;
}

void SimulationWorld::import_snapshot(std::span<const std::byte> bytes, SnapshotReadLimits policy) {
  if(bytes.size()<64 || bytes.size()>policy.maximum_bytes) throw std::invalid_argument("simulation snapshot size is invalid");
  SnapshotReader header(bytes.first(64));
  for(const auto expected:std::array<std::uint8_t,8>{'O','F','F','S','I','M',0,0}) if(header.integer<std::uint8_t>()!=expected) throw std::invalid_argument("simulation snapshot magic is invalid");
  if(header.integer<std::uint32_t>()!=1 || header.integer<std::uint32_t>()!=1 || header.integer<std::uint32_t>()!=0x01020304U || header.integer<std::uint32_t>()!=64) throw std::invalid_argument("simulation snapshot version is unsupported");
  const auto payload_size=header.integer<std::uint64_t>(); std::array<std::uint8_t,32> expected{}; for(auto &byte:expected) byte=header.integer<std::uint8_t>();
  if(!header.exhausted() || payload_size!=bytes.size()-64U) throw std::invalid_argument("simulation snapshot length is invalid");
  const auto body=bytes.subspan(64); crypto::Sha256 checksum; checksum.update(body); if(checksum.finish()!=expected) throw std::invalid_argument("simulation snapshot checksum is invalid");
  SnapshotReader reader(body); WorldLimits limits{reader.integer<std::uint32_t>(),reader.integer<std::uint32_t>(),reader.integer<std::uint32_t>(),reader.integer<std::uint32_t>()};
  if(limits.maximum_entities==0 || limits.maximum_pending_spawns==0 || limits.maximum_pending_destroys==0 || limits.maximum_pending_events==0 || limits.maximum_entities>policy.maximum_entities || limits.maximum_pending_spawns>policy.maximum_pending_spawns || limits.maximum_pending_destroys>policy.maximum_pending_destroys || limits.maximum_pending_events>policy.maximum_pending_events) throw std::invalid_argument("simulation snapshot limits are invalid");
  SimulationWorld staged(limits); staged.tick_=reader.integer<std::uint64_t>(); staged.next_sequence_=reader.integer<std::uint64_t>(); staged.last_input_=read_snapshot_input(reader);
  if(staged.next_sequence_==0 || staged.last_input_.tick!=staged.tick_) throw std::invalid_argument("simulation snapshot clock is invalid");
  const auto read_count=[&](std::uint32_t maximum){const auto count=reader.integer<std::uint32_t>();if(count>maximum)throw std::invalid_argument("simulation snapshot count exceeds limits");return count;};
  const auto slot_count=read_count(limits.maximum_entities); staged.slots_.reserve(slot_count);
  for(std::uint32_t i=0;i<slot_count;++i) { Slot slot; slot.generation=reader.integer<std::uint32_t>(); const auto alive=reader.integer<std::uint8_t>(); if(slot.generation==0 || alive>1 || (!alive && slot.generation==1) || (alive && slot.generation==std::numeric_limits<std::uint32_t>::max())) throw std::invalid_argument("simulation snapshot slot is invalid"); slot.alive=alive!=0; for(auto &value:slot.position)value=reader.integer<std::int32_t>();slot.flags=reader.integer<std::uint32_t>();staged.slots_.push_back(slot); }
  std::unordered_set<std::uint64_t> used;
  const auto spawns=read_count(limits.maximum_pending_spawns); staged.pending_spawns_.reserve(spawns);
  for(std::uint32_t i=0;i<spawns;++i){PendingSpawn spawn;spawn.request_id=reader.integer<std::uint64_t>();if(spawn.request_id==0 || spawn.request_id>=staged.next_sequence_ || !used.insert(spawn.request_id).second)throw std::invalid_argument("simulation snapshot spawn sequence is invalid");for(auto &v:spawn.state.position)v=reader.integer<std::int32_t>();spawn.state.flags=reader.integer<std::uint32_t>();staged.pending_spawns_.push_back(spawn);}
  const auto destroys=read_count(limits.maximum_pending_destroys); staged.pending_destroys_.reserve(destroys);for(std::uint32_t i=0;i<destroys;++i)staged.pending_destroys_.push_back(read_snapshot_entity(reader));
  const auto events=read_count(limits.maximum_pending_events); staged.pending_events_.reserve(events);
  for(std::uint32_t i=0;i<events;++i){SimulationEvent event;event.tick=reader.integer<std::uint64_t>();event.sequence=reader.integer<std::uint64_t>();event.type=reader.integer<std::uint32_t>();event.source=read_snapshot_entity(reader);event.target=read_snapshot_entity(reader);event.value=reader.integer<std::int64_t>();if(event.tick<=staged.tick_ || event.sequence==0 || event.sequence>=staged.next_sequence_ || !used.insert(event.sequence).second)throw std::invalid_argument("simulation snapshot event is invalid");staged.pending_events_.push_back(event);}
  if(!reader.exhausted())throw std::invalid_argument("simulation snapshot has trailing payload bytes"); staged.rebuild_live_entities(); *this=std::move(staged);
}

} // namespace off::simulation
