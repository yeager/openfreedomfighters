#include "off/graphics/renderer_camera_registry.hpp"
#include "off/audio/sound_records.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
using off::graphics::RendererCameraRegistry;
using off::graphics::CameraRegistrationServices;
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F operation) {
  bool rejected=false;try{operation();}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"expected explicit service failure");
}
std::vector<std::uint64_t> owners(const RendererCameraRegistry& registry) {
  std::vector<std::uint64_t> result;
  for(const auto& entry:registry.entries()) result.push_back(entry.owner);
  return result;
}
struct Harness {
  RendererCameraRegistry registry;
  std::set<std::uint64_t> live{1,2,3,4,5,6,7,8,9};
  std::vector<std::string> effects;
  bool ready{};
  CameraRegistrationServices services{
    [&](std::uint64_t id){return live.contains(id);},
    [&](std::uint64_t id){effects.push_back("dimensions"+std::to_string(id));},
    [&]{effects.push_back("ready");return ready;},
    [&](std::uint64_t id){effects.push_back("admit"+std::to_string(id));}};
  void add(std::uint64_t id,float key=0){registry.register_camera(id,key,services);}
  std::uint64_t at(std::size_t index){return registry.camera_at(index,services.live_owner);}
  void mixed() {add(1,0);add(2,2);add(3,0);add(4,2);add(5,0);}
};
}
int main() {
 try {
  {
    Harness h;check(h.at(0)==0 && h.registry.entries().empty(),"fresh registry empty");
    h.add(1);h.add(2,-0.0F);h.add(3);
    check(owners(h.registry)==std::vector<std::uint64_t>{3,2,1},"equal zero keys insert using retained cursor");
    check(std::bit_cast<std::uint32_t>(h.registry.entries()[1].key)==0x80000000U,"negative zero key retained");
    h.live.erase(1);h.effects.clear();h.add(2,99);
    check(owners(h.registry)==std::vector<std::uint64_t>{3,2,1} && h.effects.empty(),"duplicate neither prunes nor notifies nor changes key");
    h.add(4);
    check(owners(h.registry)==std::vector<std::uint64_t>{4,3,2,1},"duplicate leaves insertion cursor unchanged");
    check(h.at(0)==4 && owners(h.registry)==std::vector<std::uint64_t>{4,3,2},"query prunes all stale entries");
    check(h.at(99)==0,"out of range index has no owner");
  }
  {
    Harness h;h.mixed();
    check(owners(h.registry)==std::vector<std::uint64_t>{3,5,1,4,2},"mixed fixture uses cached cursor, not stable sort or first equal");
    h.live.erase(3);check(h.at(0)==5,"remove noncursor before cursor");
    h.add(6);check(owners(h.registry)==std::vector<std::uint64_t>{6,5,1,4,2},"noncursor removal preserves cursor");
  }
  {
    Harness h;h.mixed();h.live.erase(5);check(h.at(0)==3,"cursor entry removed");
    h.add(6,2);check(owners(h.registry)==std::vector<std::uint64_t>{3,1,6,4,2},"absent cursor begins from first and reaches first equal");
    h.live.erase(3);h.live.erase(1);h.live.erase(4);h.live.erase(2);h.live.erase(6);
    check(h.at(0)==0 && h.registry.entries().empty(),"adjacent stale handles swept without skips");
    h.add(7,-5);h.add(8,-10);h.add(9,20);
    check(owners(h.registry)==std::vector<std::uint64_t>{8,7,9},"insertion before first and append after empty prune");
  }
  {
    Harness h;
    for(float key:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
      rejects([&]{h.add(1,key);});
    rejects([&]{h.add(0);});rejects([&]{h.add(100);});
    check(!h.registry.failed() && h.registry.entries().empty() && h.effects.empty(),"preinsertion validation is recoverable and has no effects");
    h.add(1);check(h.effects==std::vector<std::string>{"dimensions1","ready"},"unready backend still receives dimensions");
    h.ready=true;h.effects.clear();h.add(2);
    check(h.effects==std::vector<std::string>{"dimensions2","ready","admit2"},"dimensions precede backend gate and view admission");
  }
  {
    Harness h;h.ready=true;h.services.admit_view={};
    rejects([&]{h.add(1);});
    check(h.registry.failed() && owners(h.registry)==std::vector<std::uint64_t>{1} &&
          h.effects==std::vector<std::string>{"dimensions1","ready"},"missing ready view hook preserves membership prefix and poisons");
    rejects([&]{h.add(1);});rejects([&]{(void)h.at(0);});
  }
  {
    Harness h;h.services.notify_dimensions=[&](std::uint64_t){throw std::runtime_error("dimensions");};
    rejects([&]{h.add(1);});check(h.registry.failed() && owners(h.registry)==std::vector<std::uint64_t>{1} && h.effects.empty(),"dimension failure skips backend and keeps inserted prefix");
  }
  {
    Harness h;h.services.notify_dimensions=[&](std::uint64_t){
      rejects([&]{h.add(2);});rejects([&]{(void)h.at(0);});rejects([&]{(void)h.registry.entries();});
    };
    h.add(1);check(!h.registry.failed() && h.at(0)==1,"caught reentry leaves registration healthy");
    h.services.notify_dimensions=[&](std::uint64_t){h.add(3);};
    rejects([&]{h.add(2);});check(h.registry.failed() && owners(h.registry)==std::vector<std::uint64_t>{2,1},"uncaught insertion reentry poisons after published prefix");
  }
  {
    Harness h;h.add(1);h.add(2);h.add(3);
    rejects([&]{(void)h.registry.camera_at(0,[&](std::uint64_t id){
      if(id==3)return false;
      if(id==2)throw std::runtime_error("lookup");
      return true;
    });});
    check(h.registry.failed() && owners(h.registry)==std::vector<std::uint64_t>{2,1},"lookup failure preserves preceding prune and poisons");
  }
  {
    off::audio::SoundRecordRegistry sound;
    Harness cameras;cameras.add(1);cameras.add(2);cameras.add(3);
    check(sound.listener_handle()==0 && !sound.listener_offsets(),"fresh listener absent, offsets not invented");
    const auto fallback=[&]{return cameras.at(0);};
    const auto live=[&](std::uint64_t id){return cameras.live.contains(id);};
    check(sound.resolve_listener(live,fallback)==3 && sound.listener_handle()==0,"fallback is actual registry index zero without assigning explicit state");
    // No enabled predicate exists in the membership/selection service: a live
    // camera with disabled rendering is still eligible as registry index zero.
    cameras.live.erase(3);check(sound.resolve_listener(live,fallback)==2,"fallback prunes stale first identity");
    check(sound.resolve_listener(live,{})==0,"absent first renderer gives no listener");
    sound.set_listener(1,live);
    check(sound.listener_handle()==1 && sound.listener_offsets()==std::array<float,3>{0,0,0},"explicit setter retains handle and clears offsets");
    int calls=0;int fallbacks=0;
    check(sound.resolve_listener([&](std::uint64_t id){++calls;return id==1;},[&]{++fallbacks;return 2;})==1 && calls==2 && fallbacks==0,
          "valid explicit listener resolves twice and bypasses renderer");
    rejects([&]{sound.set_listener(99,live);});rejects([&]{sound.set_listener(0,live);});
    check(sound.listener_handle()==1 && sound.listener_offsets()==std::array<float,3>{0,0,0},"invalid setter preserves handle and offsets");
    calls=0;
    check(sound.resolve_listener([&](std::uint64_t){return ++calls==1;},[&]{++fallbacks;return 2;})==0 && calls==2 && fallbacks==0,
          "identity lost between explicit resolutions yields absent, not fallback");
    cameras.live.erase(1);check(sound.resolve_listener(live,fallback)==2 && sound.listener_handle()==1,"stale explicit handle uses live renderer fallback without rewriting state");
    sound.clear_scene_listener();check(sound.listener_handle()==0 && sound.listener_offsets()==std::array<float,3>{0,0,0},"scene clear does not invent offset reset");
    check(sound.resolve_listener(live,[]{return std::uint64_t{999};})==0,"stale returned renderer identity rejected");
    rejects([&]{(void)sound.resolve_listener({},fallback);});
    sound.set_listener(2,live);
    rejects([&]{(void)sound.resolve_listener([&](std::uint64_t){sound.clear_scene_listener();return true;},fallback);});
    check(sound.listener_handle()==2,"listener reentry cannot clear retained selection");
  }
  std::cout<<"Camera registry cursor ordering, lifetime, service boundaries and listener selection verified.\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
