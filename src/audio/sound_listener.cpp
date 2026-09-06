#include "off/audio/sound_records.hpp"
#include <stdexcept>

namespace off::audio {
namespace {
struct Guard {
  bool& busy;
  explicit Guard(bool& value):busy(value) {
    if(busy) throw std::runtime_error("Sound listener selection cannot reenter");
    busy=true;
  }
  ~Guard() {busy=false;}
};
}
void SoundRecordRegistry::set_listener(std::uint64_t owner,const LiveOwner& supplied) {
  Guard guard(listener_busy_);
  const auto live=supplied;
  if(!live || !owner || !live(owner)) throw std::runtime_error("Sound listener owner is not live");
  listener_=owner;
  listener_offsets_=std::array<float,3>{0,0,0};
}
void SoundRecordRegistry::clear_scene_listener() {
  Guard guard(listener_busy_);
  listener_=0; // No unestablished offset reset is implied by scene initialization.
}
std::uint64_t SoundRecordRegistry::resolve_listener(const LiveOwner& supplied,
    const std::function<std::uint64_t()>& first_renderer_camera) const {
  Guard guard(listener_busy_);
  const auto live=supplied;
  if(!live) throw std::runtime_error("Sound listener requires the live owner registry");
  if(listener_ && live(listener_)) return live(listener_)?listener_:0;
  if(!first_renderer_camera) return 0; // No first renderer is available.
  const auto owner=first_renderer_camera();
  return owner && live(owner)?owner:0;
}
} // namespace off::audio
