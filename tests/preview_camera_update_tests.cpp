#include "off/runtime/application_services.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
  bool rejected=false; try { action(); } catch(const std::runtime_error&) { rejected=true; }
  check(rejected,"expected preview update rejection");
}
off::graphics::PreviewCameraPose camera() { return {{0,0,1,0,1,0,1,0,0},{0,50,-200},0x10000}; }
bool near(float a,float b) { return std::abs(a-b)<0.000002F; }
}
int main() {
  try {
    off::runtime::ApplicationServices application(
      off::runtime::ClockExecutionPolicy::no_recording_or_replay,
      off::runtime::make_monotonic_clock_samples());
    auto& update=application.preview_camera_update();
    auto first=camera(), second=camera();
    off::graphics::PreviewCameraInput input{{100,100},1,{}, {},false};
    unsigned queued=0;
    const auto queue=[&](auto& live) {
      check(&live==&first || &live==&second,"queue receives the original live resource");
      check((live.resource_flags&0x100000U)!=0,"resource dirty bit is visible before queue insertion");
      ++queued;
      rejects([&] { update.run(live,input,[](auto&) {}); });
    };
    update.toggle_latch=true; update.movement_scale=-5; update.secondary_scale=0;
    update.run(first,input,queue);
    check(queued==0 && first.basis==camera().basis && !update.toggle_latch &&
          update.movement_scale==0.01F && update.secondary_scale==0.01F &&
          update.previous_pointer()==std::optional{input.pointer},
          "first-use baseline, shared clamp and unchanged setter fast path");
    input.pointer[0]=101;
    update.run(first,input,queue);
    const float sine=static_cast<float>(std::sin(double(0.2F)));
    const float cosine=static_cast<float>(std::cos(double(0.2F)));
    check(queued==1 && near(first.basis[0],sine) && near(first.basis[2],cosine) &&
          near(first.basis[6],cosine) && near(first.basis[8],-sine) &&
          first.position==camera().position && first.resource_flags==0x110000U,
          "real pointer motion rotates the permuted basis without numeric translation");
    update.run(second,input,queue);
    check(queued==1 && second.basis==camera().basis,"second camera shares the published pointer sample");
    input.pointer[0]=102;
    update.run(second,input,queue);
    check(queued==2 && second.basis==first.basis,"shared history rotates another camera by one increment");
    input.pointer[0]=103;
    update.run(first,input,queue);
    check(queued==3 && near(first.basis[0],static_cast<float>(std::sin(0.4))) &&
          near(first.basis[6],static_cast<float>(std::cos(0.4))),"successive rotations accumulate on the live basis");
    const auto before=first.basis;
    input.raw_crt_delta=0; input.pointer={999,-50};
    update.run(first,input,queue);
    check(queued==3 && first.basis==before && update.previous_pointer()==std::optional{input.pointer},
          "zero raw delta publishes pointer history without rotation");
    for(std::size_t i=0;i<input.held.size();++i) {
      auto bad=input; bad.held[i]=true;
      rejects([&] { update.run(first,bad,queue); });
    }
    for(std::size_t i=0;i<input.edges.size();++i) {
      auto bad=input; bad.edges[i]=1;
      rejects([&] { update.run(first,bad,queue); });
      bad.edges[i]=2; update.run(first,bad,queue);
    }
    auto bad=input; bad.collision_visualization=true;
    rejects([&] { update.run(first,bad,queue); });
    bad=input; bad.pointer[0]=std::numeric_limits<float>::infinity();
    rejects([&] { update.run(first,bad,queue); });
    rejects([&] { update.run(first,input,{}); });
    check(queued==3 && first.basis==before && update.previous_pointer()==std::optional{input.pointer},
          "invalid and unsupported inputs do not mutate live state");
    input.raw_crt_delta=1; input.pointer[1]+=1;
    rejects([&] { update.run(first,input,[](auto&) { throw std::runtime_error("queue failed"); }); });
    check(first.basis!=before && update.previous_pointer()==std::optional{input.pointer},
          "queue failure preserves completed transform and pointer publication");
    update.run(first,input,queue); // Guard is released; equal transform needs no queue.
    check(queued==3,"retrying unchanged input does not manufacture queue work");
    std::int32_t milliseconds=0;
    off::runtime::ApplicationServices clocked(
      off::runtime::ClockExecutionPolicy::no_recording_or_replay,
      {[] { return std::int64_t{0}; },[&] { return milliseconds; }});
    auto live=camera();
    unsigned clock_queues=0;
    const auto clock_queue=[&](auto&) { ++clock_queues; };
    rejects([&] { clocked.update_preview_camera(live,input,clock_queue); });
    clocked.reset_clock(); clocked.clock().assign_crt_mode(true);
    clocked.clock().set_rate(0);
    input.pointer={0,0}; input.raw_crt_delta=0;
    clocked.update_preview_camera(live,input,clock_queue);
    milliseconds=1; clocked.advance_crt(); clocked.clock().publish_scene(true);
    input.pointer={1,0};
    clocked.update_preview_camera(live,input,clock_queue);
    check(clocked.clock().state().scene_delta==0 && clocked.clock().state().raw_delta>0 &&
          clock_queues==1 && live.basis!=camera().basis,
          "application binding uses raw CRT delta, not supplied or frozen/scaled scene delta");
    off::graphics::PreviewCameraUpdate coupled;
    auto rotated=camera();
    off::graphics::PreviewCameraInput motion{{0,0},1,{}, {},false};
    coupled.run(rotated,motion,[](auto&) {});
    motion.pointer={2,1};
    coupled.run(rotated,motion,[](auto&) {});
    // Independent closed-form X/Y rotation matrix for the three identity axes.
    // Tolerance acknowledges native transcendental interoperability, not a
    // claim of exact equality to the original math-library entry points.
    const double sa=std::sin(-0.2), ca=std::cos(-0.2), sb=std::sin(-0.4), cb=std::cos(-0.4);
    const std::array<float,9> expected{
      float(-sb),float(sa*cb),float(ca*cb),0,float(ca),float(-sa),
      float(cb),float(sa*sb),float(ca*sb)};
    for(std::size_t i=0;i<expected.size();++i)
      check(near(rotated.basis[i],expected[i]),"coupled pitch/yaw follows reviewed basis mapping and rotation order");
    const auto prior=rotated.basis;
    motion.pointer={3,1}; motion.raw_crt_delta=std::numeric_limits<float>::max();
    // Large finite angles remain a numerical interoperability boundary, not
    // implicit clamping. Overflowing the pointer product must reject instead.
    motion.pointer[0]=std::numeric_limits<float>::max();
    rejects([&] { coupled.run(rotated,motion,[](auto&) {}); });
    check(rotated.basis==prior && coupled.previous_pointer()==std::optional{motion.pointer},
          "arithmetic failure retains published history but does not publish a partial pose");
    for(const bool negative_basis:{false,true}) {
      off::graphics::PreviewCameraUpdate signed_update;
      auto signed_pose=camera(); signed_pose.position={-0.0F,-0.0F,-0.0F};
      if(negative_basis) signed_pose.basis.fill(-1.0F);
      off::graphics::PreviewCameraInput sample{{0,0},1,{}, {},false};
      signed_update.run(signed_pose,sample,[](auto&) {});
      for(const auto value:signed_pose.position) check(std::signbit(value),"baseline preserves negative zero position");
      sample.pointer[0]=1;
      signed_update.run(signed_pose,sample,[](auto&) {});
      for(const auto value:signed_pose.position)
        check(value==0 && std::signbit(value)==negative_basis,
              "pointer-only zero translation preserves reviewed signed-zero products and additions");
    }
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
