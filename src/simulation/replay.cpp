#include "off/simulation/replay.hpp"
#include <stdexcept>
namespace off::simulation {
void SimulationReplayRecorder::begin(SimulationWorld& world, SimulationReplayRecorderLimits limits) {
  if(active() || !limits.maximum_inputs || !limits.maximum_commands || !limits.maximum_checkpoints) throw std::runtime_error("replay recorder limits are invalid");
  limits_=limits; inputs_.clear(); checkpoints_.clear(); inputs_.reserve(limits.maximum_inputs); checkpoints_.reserve(limits.maximum_checkpoints);
  capture_.begin(world,limits.maximum_commands); next_tick_=world.tick()+1;
}
void SimulationReplayRecorder::record_completed_step(const SimulationWorld& world,const InputSnapshot& input) {
  if(!active() || input.tick!=next_tick_ || world.tick()!=input.tick || inputs_.size()>=limits_.maximum_inputs || checkpoints_.size()>=limits_.maximum_checkpoints) throw std::runtime_error("replay completed step is invalid");
  inputs_.push_back(input); checkpoints_.push_back({input.tick,world.state_hash()}); ++next_tick_;
}
SimulationReplay SimulationReplayRecorder::finish() {
  if(!active() || capture_.invalidated() || capture_.commands().size()>limits_.maximum_commands || inputs_.size()!=checkpoints_.size()) throw std::runtime_error("replay recorder cannot finish");
  SimulationReplay result{{capture_.initial_snapshot().begin(),capture_.initial_snapshot().end()},{inputs_.begin(),inputs_.end()},{capture_.commands().begin(),capture_.commands().end()},{checkpoints_.begin(),checkpoints_.end()}};
  capture_.end(); inputs_.clear(); checkpoints_.clear(); next_tick_=0; return result;
}
} // namespace off::simulation
