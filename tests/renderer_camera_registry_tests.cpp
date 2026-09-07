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
using off::graphics::RendererCameraViewAdmission;
using off::graphics::RendererCameraViewAdmissionServices;
using off::graphics::RendererViewRectangle;
using off::graphics::RendererViewState;
using off::graphics::RendererPendingCameraQueue;
using off::graphics::RendererRegistryReplay;
using off::graphics::RendererRegistryReplayServices;
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
struct ViewHarness {
  RendererCameraViewAdmission admission;
  std::vector<std::string> effects;
  bool has_backend{}, backend_ready{}, ready{};
  std::size_t pending{}, views{};
  std::optional<RendererViewState> state;
  std::int32_t width{1280}, height{720};
  RendererCameraViewAdmissionServices services() {
    return {
      [&]{ effects.push_back("backend"); return has_backend; },
      [&]{ effects.push_back("ready"); return backend_ready; },
      [&]{ effects.push_back("state"); return state; },
      [&]{ effects.push_back("width"); return width; },
      [&]{ effects.push_back("height"); return height; },
      [&](RendererViewRectangle rectangle) {
        effects.push_back("create:"+std::to_string(rectangle.left)+","+std::to_string(rectangle.top)+","+
            std::to_string(rectangle.width)+","+std::to_string(rectangle.height));
        state={7}; return *state;
      },
      [&](RendererViewState){ effects.push_back("state-ready"); return ready; },
      [&](RendererViewState) { return pending; },
      [&](RendererViewState value,std::uint64_t camera,std::int32_t priority) { effects.push_back("pending:"+std::to_string(value.value)+":"+std::to_string(camera)+":"+std::to_string(priority)); },
      [&](RendererViewState) { return views; },
      [&](RendererViewState value,std::uint64_t camera) { effects.push_back("allocate:"+std::to_string(value.value)+":"+std::to_string(camera)); return std::uint64_t{12}; },
      [&](std::uint64_t view,std::uint64_t camera) { effects.push_back("associate:"+std::to_string(view)+":"+std::to_string(camera)); },
      [&](std::uint64_t view) { effects.push_back("backend-records:"+std::to_string(view)); },
      [&](std::uint64_t view,std::int64_t key) { effects.push_back("insert:"+std::to_string(view)+":"+std::to_string(key)); },
      [&](std::uint64_t view) { effects.push_back("use:"+std::to_string(view)); },
      [&](RendererViewState value) { effects.push_back("renumber:"+std::to_string(value.value)); }
    };
  }
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
    RendererPendingCameraQueue pending;
    pending.append({7},9,42);pending.append({7},10,-2);
    std::vector<std::string> effects;
    pending.materialize({7},{
      [&](RendererViewState state) { effects.push_back("initialize:"+std::to_string(state.value)); return true; },
      [&](std::uint64_t camera,std::int32_t priority) { effects.push_back("admit:"+std::to_string(camera)+":"+std::to_string(priority)); }});
    check(effects==std::vector<std::string>{"initialize:7","admit:9:42","admit:10:-2"} && pending.entries().empty(),
          "pending materialization initializes then preserves stored admission order before clearing");
    RendererPendingCameraQueue failed;failed.append({8},1,0);failed.append({8},2,0);
    rejects([&]{failed.materialize({8},{[](RendererViewState){return true;},[&](std::uint64_t camera,std::int32_t){if(camera==2) throw std::runtime_error("fail");}});});
    check(failed.failed() && failed.entries()==std::vector<off::graphics::RendererPendingCamera>{{1,0},{2,0}},
          "failed materialization retains all pending entries instead of clearing a prefix");
    RendererPendingCameraQueue cap;for(std::uint64_t camera=1;camera<=16;++camera) cap.append({9},camera,0);
    rejects([&]{cap.append({9},17,0);});check(cap.failed() && cap.entries().size()==16,"pending cap keeps its bounded prefix and becomes incomplete");
  }
  {
    ViewHarness h;h.has_backend=true;h.backend_ready=true;h.state={7};
    RendererPendingCameraQueue pending;auto services=h.services();
    services.pending_count=[&](RendererViewState){return pending.entries().size();};
    services.queue_pending=[&](RendererViewState state,std::uint64_t camera,std::int32_t priority){pending.append(state,camera,priority);};
    h.admission.admit(9,-42,services);
    check(pending.entries()==std::vector<off::graphics::RendererPendingCamera>{{9,-42}},"non-ready admission preserves priority for later state materialization");
    h.ready=true;pending.materialize({7},{[](RendererViewState){return true;},[&](std::uint64_t camera,std::int32_t priority){h.admission.admit(camera,priority,services);}});
    check(pending.entries().empty() && h.effects.back()=="renumber:7","pending queue composes with the normal ready admission instead of synthesizing a view");
  }
  {
    Harness h;h.add(1);h.add(2);h.add(3);h.live.erase(2);
    RendererRegistryReplay replay;std::vector<std::string> effects;
    RendererRegistryReplayServices services{
      [&]{effects.push_back("setup");return true;},
      [&]{effects.push_back("initial-ready");return false;},
      [&]{effects.push_back("initialize");return true;},
      [&]{effects.push_back("state");return std::optional<RendererViewState>{};},
      [&](std::uint64_t owner)->std::optional<off::graphics::RendererRegistryReplayCamera>{
        effects.push_back("resolve:"+std::to_string(owner));
        if(!h.live.contains(owner)) return std::nullopt;
        return off::graphics::RendererRegistryReplayCamera{owner,static_cast<std::int32_t>(owner*10)};},
      [&](std::uint64_t camera,std::int32_t priority){effects.push_back("admit:"+std::to_string(camera)+":"+std::to_string(priority));}};
    replay.initialize(h.registry,services);
    check(effects==std::vector<std::string>{"setup","initial-ready","initialize","state","resolve:3","admit:3:30","resolve:2","resolve:1","admit:1:10"} &&
          owners(h.registry)==std::vector<std::uint64_t>{3,2,1},"initially-unready replay visits retained registry order without pruning stale cameras");
    RendererRegistryReplay existing;effects.clear();auto existing_services=services;
    existing_services.state_zero=[&]{effects.push_back("state");return std::optional<RendererViewState>{{7}};};
    existing.initialize(h.registry,existing_services);
    check(effects==std::vector<std::string>{"setup","initial-ready","initialize","state"},"existing state suppresses registry replay");
    RendererRegistryReplay ready;effects.clear();auto ready_services=services;
    ready_services.backend_ready_before_initialization=[&]{effects.push_back("initial-ready");return true;};
    ready.initialize(h.registry,ready_services);
    check(effects==std::vector<std::string>{"setup","initial-ready"},"initially-ready branch neither initializes nor traverses");
    RendererRegistryReplay bad;auto bad_services=services;bad_services.setup_renderer=[] {return false;};
    rejects([&]{bad.initialize(h.registry,bad_services);});check(bad.failed(),"failed replay setup remains explicit incomplete state");
  }
  {
    ViewHarness h;h.has_backend=true;h.backend_ready=true;h.state={8};h.ready=false;h.pending=16;
    rejects([&]{h.admission.admit(1,0,h.services());});check(h.admission.failed() && h.effects==std::vector<std::string>{"backend","ready","state","state-ready"},
          "non-ready state rejects pending overflow before host queue mutation");
    ViewHarness views;views.has_backend=true;views.backend_ready=true;views.state={8};views.ready=true;views.views=16;
    rejects([&]{views.admission.admit(1,0,views.services());});check(views.admission.failed() && views.effects==std::vector<std::string>{"backend","ready","state","state-ready"},
          "ready state rejects view overflow before allocation");
  }
  {
    ViewHarness h;
    h.admission.admit(9,42,h.services());
    check(h.effects==std::vector<std::string>{"backend"},"absent backend has no state or pending effect");
    h.has_backend=true; h.effects.clear(); h.admission.admit(9,42,h.services());
    check(h.effects==std::vector<std::string>{"backend","ready"},"unready backend has no state or pending effect");
    h.backend_ready=true; h.effects.clear(); h.admission.admit(9,42,h.services());
    check(h.effects==std::vector<std::string>{"backend","ready","state","width","height","create:0,0,1280,720","state-ready","pending:7:9:42"},
          "new non-ready state creates a full rectangle then queues without allocating");
    h.effects.clear(); h.admission.admit(9,42,h.services());
    check(h.effects==std::vector<std::string>{"backend","ready","state","state-ready","pending:7:9:42"},
          "non-ready admission preserves pending insertion order without deduplication");
    h.ready=true; h.effects.clear(); h.admission.admit(10,-42,h.services());
    check(h.effects==std::vector<std::string>{"backend","ready","state","state-ready","allocate:7:10","associate:12:10","backend-records:12","insert:12:42","use:12","renumber:7"},
          "existing ready state retains rectangle and completes checked allocation ordering");
  }
  {
    ViewHarness h; h.has_backend=true; h.backend_ready=true; h.state={8}; h.ready=true;
    auto services=h.services(); services.allocate_view={};
    rejects([&]{h.admission.admit(1,std::numeric_limits<std::int32_t>::min(),services);});
    check(h.admission.failed() && h.effects==std::vector<std::string>{"backend","ready","state","state-ready"},
          "missing allocation service fails without inventing a view");
    rejects([&]{h.admission.admit(2,0,h.services());});
    ViewHarness negated; negated.has_backend=true; negated.backend_ready=true; negated.state={8}; negated.ready=true;
    negated.admission.admit(1,std::numeric_limits<std::int32_t>::min(),negated.services());
    check(negated.effects[7]=="insert:12:2147483648","priority negation widens signed minimum");
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
