#include "off/simulation/world_command_capture.hpp"

#include <limits>
#include <stdexcept>

namespace off::simulation {
WorldCommandCapture::~WorldCommandCapture() { end(); }
void WorldCommandCapture::begin(SimulationWorld& world, std::size_t maximum_commands) {
  if(active() || maximum_commands==0) throw std::runtime_error("world command capture is already active or unbounded");
  auto snapshot=world.export_snapshot();
  commands_.clear(); commands_.reserve(maximum_commands);
  initial_snapshot_=std::move(snapshot); maximum_commands_=maximum_commands; next_ordinal=1; invalidated_=false;
  world.attach_command_capture(*this); world_=&world;
}
void WorldCommandCapture::end() noexcept { if(world_) world_->detach_command_capture(*this); world_=nullptr; }
void WorldCommandCapture::preflight() const { if(!active() || commands_.size()>=maximum_commands_ || next_ordinal==0) throw std::runtime_error("world command capture cannot record another mutation"); }
void WorldCommandCapture::accepted_spawn(std::uint64_t tick, SpawnState state,std::uint64_t request) noexcept { commands_.push_back({tick,next_ordinal++,WorldCommandKind::spawn,state,{},{},request}); }
void WorldCommandCapture::accepted_destroy(std::uint64_t tick, EntityId entity) noexcept { commands_.push_back({tick,next_ordinal++,WorldCommandKind::destroy,{},entity,{},{}}); }
void WorldCommandCapture::accepted_event(std::uint64_t tick, SimulationEvent event) noexcept { commands_.push_back({tick,next_ordinal++,WorldCommandKind::event,{},{},event,event.sequence}); }
void WorldCommandCapture::invalidate_from_world() noexcept { invalidated_=true; world_=nullptr; }
} // namespace off::simulation
