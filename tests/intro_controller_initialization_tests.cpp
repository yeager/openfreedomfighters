#include "off/graphics/intro_controller_initialization.hpp"
#include "off/graphics/renderer_frame.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace off::graphics;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool caught=false; try { f(); } catch(const std::runtime_error&) { caught=true; } check(caught,"expected failure"); }
struct Fixture {
  IntroControllerInitialization controller;
  RendererFrameClock frame_clock;
  std::vector<std::string> log;
  std::map<std::string,std::uint32_t> global,scene;
  std::uint32_t clock=99;
  std::int32_t volume=50;
  bool input=true,mode=false;
  std::uint64_t next_renderer=10;
  std::int32_t width=640,height=448;
  IntroControllerPhaseTwoServices services;
  Fixture() {
    services.input_manager_exists=[&] { log.push_back("input?"); return input; };
    services.register_movie_control_action_map=[&] { log.push_back("map"); };
    services.assign_engine_clock_mode=[&](bool value) { log.push_back("mode"); mode=value; };
    services.query_global_property=[&](std::string_view key,std::uint32_t& output) {
      check(output==0,"property caller output must start zero"); log.push_back(std::string(key));
      if(auto it=global.find(std::string(key));it!=global.end()) output=it->second;
    };
    services.current_audio_volume=[&] { log.push_back("getvolume"); return volume; };
    services.request_audio_volume=[&](std::uint32_t value) { log.push_back("volume:"+std::to_string(value)); clock=1000; };
    services.scene_integer_clock=[&] { log.push_back("clock"); return clock; };
    services.first_renderer=[&] { log.push_back("renderer"); return next_renderer++; };
    services.renderer_height=[&](std::uint64_t id) { check(id==10,"height renderer"); log.push_back("height"); width=800; return height; };
    services.renderer_width=[&](std::uint64_t id) { check(id==10,"width renderer"); log.push_back("width"); return width; };
    services.set_viewport=[&](std::uint64_t id,const PictureDeviceViewport& v) {
      check(id==10&&v.x==0&&v.y==0&&v.width==800&&v.height==448&&v.minimum_depth==0&&v.maximum_depth==1,"live viewport dimensions");
      check(controller.deadline_assigned(),"deadline before viewport"); log.push_back("viewport");
    };
    services.renderer_has_stencil=[&](std::uint64_t id) { log.push_back("stencil:"+std::to_string(id)); return id==11; };
    services.clear=[&](std::uint64_t id,const PictureViewClear& c) {
      check(c.color&&c.depth&&c.stencil==(id==11)&&c.packed_color==0&&c.depth_value==1&&c.stencil_value==0,"clear planes and values");
      log.push_back("clear:"+std::to_string(id));
    };
    services.present=[&](std::uint64_t id) { log.push_back("present:"+std::to_string(id)); return IntroPresentationResult::presented; };
  }
};
}
int main() {
  try {
    Fixture f;
    check(f.controller.deadline()==0&&!f.controller.deadline_assigned()&&!f.controller.phase_two_completed(),"fresh state is not admission");
    f.global={{"SoundReadFromMem",1},{"SfxV",37}};
    f.controller.run_phase_two(f.services);
    check(f.log==std::vector<std::string>{"input?","map","mode","SoundReadFromMem","SfxV","volume:37","clock","renderer","height","width","viewport","renderer","stencil:11","clear:11","renderer","present:12","renderer","stencil:13","clear:13","renderer","present:14"},"phase two exact ordered live service calls");
    check(f.controller.deadline()==3048&&f.controller.phase_two_completed()&&f.controller.clock_mode_assignment_completed()&&f.mode,"retained deadline uses post-audio clock");
    check(f.frame_clock.value()==1,"presentation does not increment RendererFrame clock");
    f.next_renderer=10; f.global.clear(); f.clock=5000;
    f.controller.run_phase_two(f.services);
    check(f.controller.deadline()==7048&&f.controller.phase_two_completed(),"explicit second successful phase-two invocation resamples deadline");
    f.services.current_audio_volume=[]()->std::int32_t { throw std::runtime_error("second initialization audio failure"); };
    rejects([&] { f.controller.run_phase_two(f.services); });
    check(f.controller.deadline()==7048&&f.controller.deadline_assigned()&&!f.controller.phase_two_completed()&&f.controller.failed(),"later invocation failure preserves previously assigned deadline");
    for(std::uint32_t value:{0U,100U,101U,UINT32_MAX}) {
      Fixture b; b.global={{"SoundReadFromMem",1},{"SfxV",value}};
      b.controller.run_phase_two(b.services);
      check(b.log[5]=="volume:"+std::to_string(value<=100?value:100),"unsigned SfxV clamps to100");
    }
    for(std::int32_t volume:{-1,0,100,101}) {
      Fixture b; b.input=false; b.volume=volume; b.scene={{"SoundReadFromMem",1},{"SfxV",88}};
      b.clock=UINT32_MAX-1023;
      b.controller.run_phase_two(b.services);
      check(b.log[1]=="mode"&&b.log[2]=="SoundReadFromMem"&&b.log[3]=="getvolume","scene store cannot substitute global store, absent input skips registration");
      const bool outside=volume<0||volume>100;
      check(b.controller.deadline()==(outside?3048U:1024U),"wrapping deadline or audio-mutated live clock");
      check(b.log[4]==(outside?"volume:100":"clock"),"only out-of-range signed volume corrected");
    }
    Fixture missing; missing.services.clear={};
    rejects([&] { missing.controller.run_phase_two(missing.services); });
    check(missing.log.empty()&&!missing.controller.failed(),"missing required callback rejects before effects");
    Fixture callback_failure;
    callback_failure.services.query_global_property=[](std::string_view,std::uint32_t&) { throw std::runtime_error("property failed"); };
    rejects([&] { callback_failure.controller.run_phase_two(callback_failure.services); });
    check(callback_failure.controller.clock_mode_assignment_completed()&&!callback_failure.controller.deadline_assigned()&&callback_failure.controller.failed(),"audio prefix failure retains mode but no deadline");
    const auto prefix=callback_failure.log;
    rejects([&] { callback_failure.controller.run_phase_two(callback_failure.services); });
    check(callback_failure.log==prefix,"failed initialization cannot retry prefix");
    for(auto result:{IntroPresentationResult::device_lost,IntroPresentationResult::configuration_reset_required}) {
      Fixture broken;
      broken.services.present=[&](std::uint64_t id) { broken.log.push_back("failedpresent:"+std::to_string(id)); return result; };
      rejects([&] { broken.controller.run_phase_two(broken.services); });
      check(broken.controller.deadline()==2147&&broken.controller.deadline_assigned()&&!broken.controller.phase_two_completed()&&broken.controller.failed(),"presentation failure keeps deadline without completion");
      check(broken.log.back()=="failedpresent:12"&&broken.next_renderer==13&&broken.frame_clock.value()==1,"failure stops before second clear and never traverses scene");
    }
    Fixture dimensions; dimensions.height=-1;
    rejects([&] { dimensions.controller.run_phase_two(dimensions.services); });
    check(dimensions.controller.deadline_assigned()&&dimensions.log.back()=="width","dimension rejection follows both live queries");
    Fixture reentry;
    reentry.services.assign_engine_clock_mode=[&](bool) { rejects([&] { reentry.controller.run_phase_two(reentry.services); }); };
    reentry.controller.run_phase_two(reentry.services);
    check(reentry.controller.phase_two_completed(),"caught nested preflight rejection leaves outer execution intact");
    Fixture mutation;
    mutation.services.input_manager_exists=[&] { mutation.services.clear={}; return false; };
    mutation.controller.run_phase_two(mutation.services);
    check(mutation.controller.phase_two_completed(),"service callable snapshot survives caller table mutation");
    std::cout<<"Controller phase-two ordering, properties, deadline and presentation boundaries verified.\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
