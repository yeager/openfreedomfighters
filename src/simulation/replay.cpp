#include "off/simulation/replay.hpp"
#include <limits>
#include <stdexcept>
namespace off::simulation {
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
  if(replay.initial_snapshot.empty() || replay.inputs.size()!=replay.checkpoints.size() || replay.inputs.size()>limits.maximum_inputs || replay.commands.size()>limits.maximum_commands || replay.checkpoints.size()>limits.maximum_checkpoints) throw std::runtime_error("replay limits or shape are invalid");
  SimulationWorld staged; staged.import_snapshot(replay.initial_snapshot,limits.snapshot); const auto initial=staged.tick(); std::size_t command=0;
  for(std::size_t i=0;i<replay.inputs.size();++i) {
    const auto& input=replay.inputs[i]; const auto& checkpoint=replay.checkpoints[i];
    if(input.tick!=initial+i+1 || checkpoint.completed_tick!=input.tick) throw std::runtime_error("replay tick sequence is invalid");
    while(command<replay.commands.size() && replay.commands[command].issued_after_tick==staged.tick()) {
      const auto& item=replay.commands[command]; if(item.ordinal!=command+1) throw std::runtime_error("replay command ordinal is invalid");
      if(item.kind==WorldCommandKind::spawn) { if(staged.queue_spawn(item.spawn)!=item.accepted_identifier) throw std::runtime_error("replay spawn identifier diverged"); }
      else if(item.kind==WorldCommandKind::destroy) { if(item.accepted_identifier) throw std::runtime_error("replay destroy payload is invalid"); staged.queue_destroy(item.destroy); }
      else if(item.kind==WorldCommandKind::event) { if(item.event.sequence!=item.accepted_identifier || staged.queue_event(item.event.tick,item.event.type,item.event.source,item.event.target,item.event.value)!=item.accepted_identifier) throw std::runtime_error("replay event identifier diverged"); }
      else throw std::runtime_error("replay command kind is invalid"); ++command;
    }
    if(command<replay.commands.size() && replay.commands[command].issued_after_tick<staged.tick()) throw std::runtime_error("replay command tick is invalid");
    static_cast<void>(staged.step(input)); if(staged.state_hash()!=checkpoint.state_hash) throw std::runtime_error("replay checkpoint diverged");
  }
  if(command!=replay.commands.size()) throw std::runtime_error("replay has trailing commands"); destination.import_snapshot(staged.export_snapshot(),limits.snapshot);
}
} // namespace off::simulation
