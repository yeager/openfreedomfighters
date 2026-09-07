#include "off/simulation/replay.hpp"
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
namespace off::simulation {
namespace {
constexpr std::array<std::byte, 4> replay_magic{std::byte{'O'}, std::byte{'F'}, std::byte{'R'}, std::byte{'P'}};
constexpr std::uint32_t replay_schema_version=1;
constexpr std::size_t replay_checksum_size=crypto::Sha256Digest{}.size();
constexpr std::size_t input_size=40;
constexpr std::size_t command_size=100;
constexpr std::size_t checkpoint_size=40;

class ByteWriter final {
public:
  void u8(std::uint8_t value) { bytes_.push_back(static_cast<std::byte>(value)); }
  void u16(std::uint16_t value) { for(unsigned shift=0;shift<16;shift+=8) u8(static_cast<std::uint8_t>(value>>shift)); }
  void u32(std::uint32_t value) { for(unsigned shift=0;shift<32;shift+=8) u8(static_cast<std::uint8_t>(value>>shift)); }
  void u64(std::uint64_t value) { for(unsigned shift=0;shift<64;shift+=8) u8(static_cast<std::uint8_t>(value>>shift)); }
  void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }
  void i16(std::int16_t value) { u16(static_cast<std::uint16_t>(value)); }
  void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }
  void bytes(std::span<const std::byte> value) { bytes_.insert(bytes_.end(),value.begin(),value.end()); }
  [[nodiscard]] std::vector<std::byte> finish() && { return std::move(bytes_); }
private:
  std::vector<std::byte> bytes_;
};

class ByteReader final {
public:
  explicit ByteReader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size()-offset_; }
  [[nodiscard]] std::uint8_t u8() { require(1); return std::to_integer<std::uint8_t>(bytes_[offset_++]); }
  [[nodiscard]] std::uint16_t u16() { std::uint16_t value{}; for(unsigned shift=0;shift<16;shift+=8) value|=static_cast<std::uint16_t>(u8())<<shift; return value; }
  [[nodiscard]] std::uint32_t u32() { std::uint32_t value{}; for(unsigned shift=0;shift<32;shift+=8) value|=static_cast<std::uint32_t>(u8())<<shift; return value; }
  [[nodiscard]] std::uint64_t u64() { std::uint64_t value{}; for(unsigned shift=0;shift<64;shift+=8) value|=static_cast<std::uint64_t>(u8())<<shift; return value; }
  [[nodiscard]] std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
  [[nodiscard]] std::int16_t i16() { return std::bit_cast<std::int16_t>(u16()); }
  [[nodiscard]] std::int64_t i64() { return std::bit_cast<std::int64_t>(u64()); }
  [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) { require(count); const auto value=bytes_.subspan(offset_,count); offset_+=count; return value; }
private:
  void require(std::size_t count) const { if(count>remaining()) throw std::runtime_error("replay envelope is truncated"); }
  std::span<const std::byte> bytes_;
  std::size_t offset_{};
};

crypto::Sha256Digest digest(std::span<const std::byte> bytes) {
  crypto::Sha256 hasher; hasher.update(bytes); return hasher.finish();
}
void append_digest(ByteWriter& writer, const crypto::Sha256Digest& value) {
  for(const auto byte : value) writer.u8(byte);
}
crypto::Sha256Digest read_digest(ByteReader& reader) {
  crypto::Sha256Digest value{}; for(auto& byte : value) byte=reader.u8(); return value;
}
void validate_limits(const SimulationReplay& replay, const ReplayPlaybackLimits& limits) {
  if(!limits.maximum_inputs || !limits.maximum_commands || !limits.maximum_checkpoints || replay.initial_snapshot.empty() || replay.initial_snapshot.size()>limits.snapshot.maximum_bytes || replay.inputs.size()!=replay.checkpoints.size() || replay.inputs.size()>limits.maximum_inputs || replay.commands.size()>limits.maximum_commands || replay.checkpoints.size()>limits.maximum_checkpoints) throw std::runtime_error("replay limits or shape are invalid");
}
bool default_entity(EntityId entity) noexcept { return entity == EntityId{}; }
bool default_spawn(SpawnState state) noexcept { return state.position == std::array<std::int32_t, 3>{} && !state.flags; }
bool default_event(const SimulationEvent& event) noexcept { return event == SimulationEvent{}; }

void validate_replay_grammar(const SimulationReplay& replay, std::uint64_t initial) {
  if(!replay.inputs.empty() && initial==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("replay initial tick is exhausted");
  std::uint64_t expected_tick=initial;
  for(std::size_t index=0;index<replay.inputs.size();++index) {
    if(expected_tick==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("replay input tick is exhausted");
    ++expected_tick;
    if(replay.inputs[index].tick!=expected_tick || replay.checkpoints[index].completed_tick!=expected_tick) throw std::runtime_error("replay tick sequence is invalid");
  }
  for(std::size_t index=0;index<replay.commands.size();++index) {
    const auto& item=replay.commands[index];
    if(item.ordinal!=index+1 || item.issued_after_tick<initial || replay.inputs.empty() || item.issued_after_tick>=expected_tick) throw std::runtime_error("replay command boundary is invalid");
    if(index && item.issued_after_tick<replay.commands[index-1].issued_after_tick) throw std::runtime_error("replay command order is invalid");
    switch(item.kind) {
      case WorldCommandKind::spawn:
        if(!default_entity(item.destroy) || !default_event(item.event) || !item.accepted_identifier) throw std::runtime_error("replay spawn payload is invalid");
        break;
      case WorldCommandKind::destroy:
        if(!default_spawn(item.spawn) || !default_event(item.event) || item.accepted_identifier) throw std::runtime_error("replay destroy payload is invalid");
        break;
      case WorldCommandKind::event:
        if(!default_spawn(item.spawn) || !default_entity(item.destroy) || item.event.sequence!=item.accepted_identifier || !item.accepted_identifier || item.event.tick<=item.issued_after_tick) throw std::runtime_error("replay event payload is invalid");
        break;
      default: throw std::runtime_error("replay command kind is invalid");
    }
  }
}
void validate_replay(const SimulationReplay& replay, const ReplayPlaybackLimits& limits) {
  validate_limits(replay,limits);
  SimulationWorld staged; staged.import_snapshot(replay.initial_snapshot,limits.snapshot);
  validate_replay_grammar(replay,staged.tick());
  constexpr auto action_count=static_cast<std::uint8_t>(DigitalAction::count);
  constexpr auto valid_mask=(std::uint64_t{1}<<action_count)-1U;
  for(const auto& input : replay.inputs) if((input.held|input.pressed|input.released)&~valid_mask) throw std::runtime_error("replay input action bits are invalid");
}
} // namespace

void SimulationReplayRecorder::begin(SimulationWorld& world, SimulationReplayRecorderLimits limits) {
  if(active() || !limits.maximum_inputs || !limits.maximum_commands || !limits.maximum_checkpoints) throw std::runtime_error("replay recorder limits are invalid");
  limits_=limits; inputs_.clear(); checkpoints_.clear(); inputs_.reserve(limits.maximum_inputs); checkpoints_.reserve(limits.maximum_checkpoints);
  if(world.tick()==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("replay recorder tick is exhausted");
  capture_.begin(world,limits.maximum_commands); world_=&world; next_tick_=world.tick()+1;
}
void SimulationReplayRecorder::record_completed_step(const SimulationWorld& world,const InputSnapshot& input) {
  if(!active() || world_!=&world || input.tick!=next_tick_ || world.tick()!=input.tick || inputs_.size()>=limits_.maximum_inputs || checkpoints_.size()>=limits_.maximum_checkpoints || (!capture_.commands().empty() && capture_.commands().back().issued_after_tick>=input.tick)) throw std::runtime_error("replay completed step is invalid");
  inputs_.push_back(input); checkpoints_.push_back({input.tick,world.state_hash()}); ++next_tick_;
}
SimulationReplay SimulationReplayRecorder::finish() {
  if(!active() || capture_.invalidated() || capture_.commands().size()>limits_.maximum_commands || inputs_.size()!=checkpoints_.size()) throw std::runtime_error("replay recorder cannot finish");
  if((inputs_.empty() && !capture_.commands().empty()) || (!inputs_.empty() && !capture_.commands().empty() && capture_.commands().back().issued_after_tick>=inputs_.back().tick)) throw std::runtime_error("replay recorder has commands after its final step");
  SimulationReplay result{{capture_.initial_snapshot().begin(),capture_.initial_snapshot().end()},{inputs_.begin(),inputs_.end()},{capture_.commands().begin(),capture_.commands().end()},{checkpoints_.begin(),checkpoints_.end()}};
  capture_.end(); world_=nullptr; inputs_.clear(); checkpoints_.clear(); next_tick_=0; return result;
}
void play_replay_atomically(SimulationWorld& destination,const SimulationReplay& replay,ReplayPlaybackLimits limits) {
  validate_replay(replay,limits);
  SimulationWorld staged; staged.import_snapshot(replay.initial_snapshot,limits.snapshot); const auto initial=staged.tick(); validate_replay_grammar(replay,initial); std::size_t command=0;
  for(std::size_t i=0;i<replay.inputs.size();++i) {
    const auto& input=replay.inputs[i]; const auto& checkpoint=replay.checkpoints[i];
    while(command<replay.commands.size() && replay.commands[command].issued_after_tick==staged.tick()) {
      const auto& item=replay.commands[command];
      if(item.kind==WorldCommandKind::spawn) { if(staged.queue_spawn(item.spawn)!=item.accepted_identifier) throw std::runtime_error("replay spawn identifier diverged"); }
      else if(item.kind==WorldCommandKind::destroy) { staged.queue_destroy(item.destroy); }
      else if(item.kind==WorldCommandKind::event) { if(staged.queue_event(item.event.tick,item.event.type,item.event.source,item.event.target,item.event.value)!=item.accepted_identifier) throw std::runtime_error("replay event identifier diverged"); }
      ++command;
    }
    static_cast<void>(staged.step(input)); if(staged.state_hash()!=checkpoint.state_hash) throw std::runtime_error("replay checkpoint diverged");
  }
  if(command!=replay.commands.size()) throw std::runtime_error("replay has trailing commands"); destination.import_snapshot(staged.export_snapshot(),limits.snapshot);
}

std::vector<std::byte> serialize_replay(const SimulationReplay& replay, ReplayPlaybackLimits limits) {
  validate_replay(replay,limits);
  ByteWriter writer;
  writer.bytes(replay_magic); writer.u32(replay_schema_version); writer.u64(replay.initial_snapshot.size());
  writer.u64(replay.inputs.size()); writer.u64(replay.commands.size()); writer.u64(replay.checkpoints.size());
  writer.bytes(replay.initial_snapshot);
  for(const auto& input : replay.inputs) {
    writer.u64(input.tick); writer.u64(input.held); writer.u64(input.pressed); writer.u64(input.released);
    for(const auto axis : input.axes) writer.i16(axis);
  }
  for(const auto& command : replay.commands) {
    writer.u64(command.issued_after_tick); writer.u64(command.ordinal); writer.u8(static_cast<std::uint8_t>(command.kind));
    for(unsigned index=0;index<7;++index) writer.u8(0);
    for(const auto position : command.spawn.position) writer.i32(position); writer.u32(command.spawn.flags);
    writer.u32(command.destroy.index); writer.u32(command.destroy.generation);
    writer.u64(command.event.tick); writer.u64(command.event.sequence); writer.u32(command.event.type);
    writer.u32(command.event.source.index); writer.u32(command.event.source.generation); writer.u32(command.event.target.index); writer.u32(command.event.target.generation); writer.i64(command.event.value); writer.u64(command.accepted_identifier);
  }
  for(const auto& checkpoint : replay.checkpoints) { writer.u64(checkpoint.completed_tick); append_digest(writer,checkpoint.state_hash); }
  auto result=std::move(writer).finish(); const auto checksum=digest(result);
  for(const auto byte : checksum) result.push_back(static_cast<std::byte>(byte));
  return result;
}

SimulationReplay deserialize_replay(std::span<const std::byte> bytes, ReplayPlaybackLimits limits) {
  if(bytes.size()<replay_magic.size()+sizeof(std::uint32_t)+4*sizeof(std::uint64_t)+replay_checksum_size) throw std::runtime_error("replay envelope is truncated");
  const auto body=bytes.first(bytes.size()-replay_checksum_size); const auto expected=digest(body); ByteReader checksum_reader(bytes.last(replay_checksum_size));
  if(read_digest(checksum_reader)!=expected) throw std::runtime_error("replay envelope checksum is invalid");
  ByteReader reader(body); for(const auto expected_byte : replay_magic) if(reader.u8()!=std::to_integer<std::uint8_t>(expected_byte)) throw std::runtime_error("replay envelope magic is invalid");
  if(reader.u32()!=replay_schema_version) throw std::runtime_error("replay envelope schema is unsupported");
  const auto snapshot_size=reader.u64(), input_count=reader.u64(), command_count=reader.u64(), checkpoint_count=reader.u64();
  if(snapshot_size>limits.snapshot.maximum_bytes || input_count>limits.maximum_inputs || command_count>limits.maximum_commands || checkpoint_count>limits.maximum_checkpoints || input_count!=checkpoint_count || snapshot_size>std::numeric_limits<std::size_t>::max()) throw std::runtime_error("replay envelope limits or shape are invalid");
  const auto require_records=[&](std::uint64_t count,std::size_t size) { if(count>reader.remaining()/size) throw std::runtime_error("replay envelope is truncated"); };
  SimulationReplay replay; const auto snapshot=reader.bytes(static_cast<std::size_t>(snapshot_size)); replay.initial_snapshot.assign(snapshot.begin(),snapshot.end());
  require_records(input_count,input_size); replay.inputs.reserve(static_cast<std::size_t>(input_count));
  for(std::uint64_t index=0;index<input_count;++index) { InputSnapshot input; input.tick=reader.u64(); input.held=reader.u64(); input.pressed=reader.u64(); input.released=reader.u64(); for(auto& axis : input.axes) axis=reader.i16(); replay.inputs.push_back(input); }
  require_records(command_count,command_size); replay.commands.reserve(static_cast<std::size_t>(command_count));
  for(std::uint64_t index=0;index<command_count;++index) { CapturedWorldCommand command; command.issued_after_tick=reader.u64(); command.ordinal=reader.u64(); command.kind=static_cast<WorldCommandKind>(reader.u8()); for(unsigned reserved=0;reserved<7;++reserved) if(reader.u8()!=0) throw std::runtime_error("replay envelope reserved bytes are invalid"); for(auto& position : command.spawn.position) position=reader.i32(); command.spawn.flags=reader.u32(); command.destroy={reader.u32(),reader.u32()}; command.event.tick=reader.u64(); command.event.sequence=reader.u64(); command.event.type=reader.u32(); command.event.source={reader.u32(),reader.u32()}; command.event.target={reader.u32(),reader.u32()}; command.event.value=reader.i64(); command.accepted_identifier=reader.u64(); replay.commands.push_back(command); }
  require_records(checkpoint_count,checkpoint_size); replay.checkpoints.reserve(static_cast<std::size_t>(checkpoint_count));
  for(std::uint64_t index=0;index<checkpoint_count;++index) replay.checkpoints.push_back({reader.u64(),read_digest(reader)});
  if(reader.remaining()!=0) throw std::runtime_error("replay envelope has trailing bytes");
  validate_replay(replay,limits); return replay;
}
} // namespace off::simulation
