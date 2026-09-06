#include "off/graphics/preview_camera_component.hpp"
#include "off/graphics/fresh_intro_camera.hpp"
#include "off/runtime/application_services.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F operation) {
  bool rejected=false;try {operation();}catch(const std::runtime_error&) {rejected=true;}
  check(rejected,"expected live-variable rejection");
}
}
int main() {
 try {
  using namespace off::runtime;
  LiveVariableRegistry registry;
  std::array<LiveVariableHandle,4> retired{};
  {
    off::graphics::PreviewCameraComponent component(registry);
    const auto ids=component.handles();retired=ids;
    check(!component.collision_enabled() && component.collision_length()==1000 &&
          component.dynamic_check() && component.static_check(),"concrete constructor state");
    const std::array names{"cam_coli_check_dynamic","cam_coli_check_static","cam_coli_enable","cam_coli_len"};
    for(std::size_t i=0;i<4;++i) {
      check(registry.enumerate(names[i])==std::vector<LiveVariableHandle>{ids[i]},"exact variable names and handles");
      if(i) check(ids[i-1].identity<ids[i].identity,"registration order");
      check(registry.type(ids[i])==(i==3?LiveVariableType::floating:LiveVariableType::boolean),"typed bindings");
    }
    registry.write_bool(ids[1],false);
    check(!component.dynamic_check() && component.static_check() && !registry.read_bool(ids[0]),"static-name alias writes dynamic only");
    registry.write_bool(ids[0],true);check(registry.read_bool(ids[1]),"dynamic write visible through static alias");
    registry.write_bool(ids[2],true);registry.write_float(ids[3],-12.5F);
    check(component.collision_enabled() && component.collision_length()==-12.5F,"live finite mutations are not clamped");
    rejects([&]{registry.write_float(ids[0],1);});rejects([&]{registry.write_bool(ids[3],true);});
    rejects([&]{(void)registry.read_float(ids[0]);});rejects([&]{(void)registry.read_bool(ids[3]);});
    for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
      rejects([&]{registry.write_float(ids[3],bad);});
    check(component.collision_length()==-12.5F,"invalid float leaves storage unchanged");
    {
      off::graphics::PreviewCameraComponent duplicate(registry);
      check(registry.enumerate(names[0])==std::vector<LiveVariableHandle>{ids[0],duplicate.handles()[0]},"duplicate names retain both identities");
      registry.write_bool(duplicate.handles()[0],false);
      check(component.dynamic_check() && !duplicate.dynamic_check(),"duplicate names do not share storage");
    }
    check(registry.enumerate(names[0])==std::vector<LiveVariableHandle>{ids[0]},"duplicate teardown preserves earlier registration");
  }
  for(const auto id:retired) {
    check(!registry.contains(id),"component destruction invalidates every handle");
    rejects([&]{(void)registry.type(id);});rejects([&]{registry.write_bool(id,false);});
  }
  bool flag=false;float value=1;
  auto first=registry.bind("lease",flag);const auto id=first.handle();
  auto moved=std::move(first);check(first.handle().identity==0 && registry.contains(id),"move transfers registration");
  auto second=registry.bind("second",value);const auto old=second.handle();
  second=std::move(moved);check(!registry.contains(old) && registry.contains(id),"move assignment retires previous lease");
  second.reset();second.reset();check(!registry.contains(id),"repeated reset is inert");
  auto fresh=registry.bind("lease",flag);check(fresh.handle().identity>id.identity,"retired identity not reused");
  LiveVariableRegistry other_registry;
  auto other=other_registry.bind("lease",flag);
  check(!registry.contains(other.handle()),"foreign registry handle is not live here");
  rejects([&]{registry.write_bool(other.handle(),true);});
  auto floating=registry.bind("external",value);value=std::numeric_limits<float>::infinity();
  rejects([&]{(void)registry.read_float(floating.handle());});
  rejects([&]{(void)registry.bind("invalid",value);});rejects([&]{(void)registry.bind("",flag);});
  rejects([&]{(void)registry.read_bool({});});
  check(registry.enumerate("missing").empty(),"unknown name has no guessed winner");
  {
    std::int32_t sample=0;
    ApplicationServices application(ClockExecutionPolicy::no_recording_or_replay,
      {[]{return std::int64_t{0};},[&]{return sample;}});
    std::array<LiveVariableHandle,4> component_handles;
    off::graphics::FreshIntroCamera owner;
    const auto owner_flags=owner.flags();
    off::graphics::PreviewCameraPose pose{{0,0,1,0,1,0,1,0,0},{0,50,-200},0x400};
    off::graphics::PreviewCameraInput input{{0,0},std::numeric_limits<float>::quiet_NaN(),{},{},true};
    unsigned queued=0;
    const auto queue=[&](auto& current){
      check(&current==&pose && current.resource_flags==0x100400,"update queues actual resource with separate dirty/hide flags");
      ++queued;
    };
    {
      off::graphics::PreviewCameraComponent component(application.live_variables());
      component_handles=component.handles();
      rejects([&]{component.update(application,pose,input,queue);});
      application.reset_clock();application.clock().assign_crt_mode(true);application.clock().set_rate(0);
      component.update(application,pose,input,queue);
      off::runtime::ApplicationServices foreign_application(
        off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        off::runtime::make_monotonic_clock_samples());
      rejects([&]{component.update(foreign_application,pose,input,queue);});
      check(queued==0 && application.preview_camera_update().previous_pointer()==std::optional{input.pointer},
            "component overrides fake collision input and application overrides fake raw delta");
      sample=1000;(void)application.advance_crt();application.clock().publish_scene(true);
      input.pointer={1,0};input.raw_crt_delta=0;
      component.update(application,pose,input,queue);
      check(queued==1 && application.clock().state().raw_delta>0 && application.clock().state().scene_delta==0 &&
            pose.basis!=std::array<float,9>{0,0,1,0,1,0,1,0,0} && pose.position==std::array<float,3>{0,50,-200} &&
            owner.flags()==owner_flags && pose.resource_flags==0x100400,
            "canonical controlled raw clock drives resource rotation without mutating camera-owner flags");
      application.live_variables().write_bool(component_handles[2],true);
      input.collision_visualization=false;input.pointer={2,0};
      const auto before=pose.basis;
      rejects([&]{component.update(application,pose,input,queue);});
      check(pose.basis==before && queued==1,"live enabled collision state overrides caller false and rejects unsupported branch");
      application.live_variables().write_bool(component_handles[2],false);
      component.update(application,pose,input,queue);
      check(queued==2 && owner.flags()==owner_flags,"live collision disable restores admitted pointer path");
    }
    for(const auto handle:component_handles) {
      check(!application.live_variables().contains(handle),"application registry outlives released component handles");
      rejects([&]{application.live_variables().write_bool(handle,false);});
    }
  }
  std::cout<<"Preview camera live variables: typed mutation, aliasing, duplicate names and lease lifetime verified.\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
