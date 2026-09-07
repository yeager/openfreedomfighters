#include "off/simulation/replay.hpp"
#include <limits>
#include <stdexcept>
namespace off::simulation {
namespace {
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
  if(!limits.maximum_inputs || !limits.maximum_commands || !limits.maximum_checkpoints || replay.initial_snapshot.empty() || replay.inputs.size()!=replay.checkpoints.size() || replay.inputs.size()>limits.maximum_inputs || replay.commands.size()>limits.maximum_commands || replay.checkpoints.size()>limits.maximum_checkpoints) throw std::runtime_error("replay limits or shape are invalid");
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
} // namespace off::simulation
