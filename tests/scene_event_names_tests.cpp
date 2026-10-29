#include "off/runtime/scene_event_names.hpp"
#include "off/runtime/deferred_sequence_start_queue.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message) {if(!value) {std::cerr<<message<<'\n';std::exit(1);}}
template<class Action> void rejects(Action action) {
  try {action();} catch(const std::runtime_error&) {return;}
  check(false,"expected explicit registry rejection");
}
}
int main() {
  {
    using off::runtime::DeferredSequenceStartQueue;
    off::graphics::SceneReferenceRegistry references;
    references.append(11); references.append(22); references.append(11);
    auto scene=references.seal();
    auto owner=references.relocate(1,scene), other=references.relocate(2,scene);
    off::runtime::SceneEventNames events;
    auto event=events.declare("start"); auto event_lease=events.lease();
    DeferredSequenceStartQueue queue,foreign_queue;
    auto queue_handle=queue.handle();
    check(!foreign_queue.live(queue_handle),"foreign queue accepted handle");
    check(queue.enqueue(queue_handle,references,scene,events,event_lease,owner,event,other),"enqueue failed");
    const auto alias=references.relocate(3,scene);
    check(!queue.enqueue(queue_handle,references,scene,events,event_lease,alias,event,0),"different tokens for one owner must deduplicate");
    off::graphics::SceneReferenceRegistry foreign_references;
    foreign_references.append(11); auto foreign_scene=foreign_references.seal();
    rejects([&] { (void)queue.enqueue(queue_handle,foreign_references,foreign_scene,events,event_lease,owner,event,0); });
    rejects([&] { (void)queue.enqueue(queue_handle,references,scene,events,event_lease,0,event,0); });
    check(!queue.enqueue(queue_handle,references,scene,events,event_lease,owner,event,0),"duplicate owner accepted");
    check(queue.enqueue(queue_handle,references,scene,events,event_lease,other,event,owner),"second enqueue failed");
    unsigned calls=0;
    const auto dispatch=[&](std::uint64_t value,std::uint16_t delivered,std::uint32_t argument) {
      check(value==(calls==0?11U:22U) && delivered==event && argument==(calls==0?other:owner),"queue order or values changed"); ++calls;
    };
    check(queue.dispatch_first(queue_handle,references,events,dispatch) &&
          queue.dispatch_first(queue_handle,references,events,dispatch) && calls==2 &&
          !queue.dispatch_first(queue_handle,references,events,dispatch),"queue dispatch order failed");
    off::runtime::SceneEventNames other_events;
    const auto other_event=other_events.declare("start");
    const auto other_event_lease=other_events.lease();
    const auto foreign_owner=foreign_references.relocate(1,foreign_scene);
    check(queue.enqueue(queue_handle,references,scene,events,event_lease,owner,event,0) &&
          queue.enqueue(queue_handle,foreign_references,foreign_scene,other_events,other_event_lease,foreign_owner,other_event,0),
          "same owner identity in distinct scenes must not deduplicate");
    rejects([&] { (void)queue.dispatch_first(queue_handle,foreign_references,other_events,dispatch); });
    check(queue.size(queue_handle)==2,"foreign scene must not remove the first scene's entry");
    check(queue.dispatch_first(queue_handle,references,events,[](auto,auto,auto){}) &&
          queue.dispatch_first(queue_handle,foreign_references,other_events,[](auto,auto,auto){}),
          "each scene must dispatch only its own FIFO entry");
    {
      off::graphics::SceneReferenceRegistry destroyed_scene;
      destroyed_scene.append(77); auto destroyed_lease=destroyed_scene.seal();
      const auto destroyed_owner=destroyed_scene.relocate(1,destroyed_lease);
      off::runtime::SceneEventNames destroyed_events;
      const auto destroyed_event=destroyed_events.declare("start");
      const auto destroyed_event_lease=destroyed_events.lease();
      check(queue.enqueue(queue_handle,destroyed_scene,destroyed_lease,destroyed_events,destroyed_event_lease,
                          destroyed_owner,destroyed_event,0),"destroyed scene setup failed");
    }
    check(queue.enqueue(queue_handle,foreign_references,foreign_scene,other_events,other_event_lease,
                        foreign_owner,other_event,0),"live scene setup after destroyed scene failed");
    unsigned after_destroy=0;
    check(queue.dispatch_first(queue_handle,foreign_references,other_events,
          [&](auto value,auto,auto) { check(value==11,"wrong owner after expired prune"); ++after_destroy; }) &&
          after_destroy==1 && queue.size(queue_handle)==0,"expired front must not block a live scene");
    {
      off::graphics::SceneReferenceRegistry retired_scene;
      retired_scene.append(77); auto retired_lease=retired_scene.seal();
      const auto retired_owner=retired_scene.relocate(1,retired_lease);
      off::runtime::SceneEventNames retired_events;
      auto retired_event=retired_events.declare("start"); auto retired_event_lease=retired_events.lease();
      check(queue.enqueue(queue_handle,retired_scene,retired_lease,retired_events,retired_event_lease,retired_owner,retired_event,0),"retired front setup failed");
      retired_scene.retire();
      check(queue.enqueue(queue_handle,foreign_references,foreign_scene,other_events,other_event_lease,foreign_owner,other_event,0),"live scene setup after retirement failed");
      check(queue.dispatch_first(queue_handle,foreign_references,other_events,[](auto,auto,auto){}) && queue.size(queue_handle)==0,
            "retired live registry must not block a live scene");
    }
    {
      off::graphics::SceneReferenceRegistry live_scene;
      live_scene.append(88); auto live_lease=live_scene.seal();
      const auto live_owner=live_scene.relocate(1,live_lease);
      off::runtime::SceneEventNames cleared_events;
      auto cleared_event=cleared_events.declare("start"); auto cleared_lease=cleared_events.lease();
      check(queue.enqueue(queue_handle,live_scene,live_lease,cleared_events,cleared_lease,live_owner,cleared_event,0),"cleared front setup failed");
      cleared_events.clear(); (void)cleared_events.declare("start");
      check(queue.enqueue(queue_handle,foreign_references,foreign_scene,other_events,other_event_lease,foreign_owner,other_event,0),"live scene setup after clear failed");
      check(queue.dispatch_first(queue_handle,foreign_references,other_events,[](auto,auto,auto){}) && queue.size(queue_handle)==0,
            "cleared live event table must not block a live scene");
    }
    check(queue.enqueue(queue_handle,references,scene,events,event_lease,owner,event,0),"failure setup failed");
    rejects([&] { (void)queue.dispatch_first(queue_handle,references,events,
        [](auto,auto,auto) { throw std::runtime_error("dispatch failed"); }); });
    check(queue.size(queue_handle)==0,"failed dispatch must leave the popped entry removed");
    check(queue.enqueue(queue_handle,references,scene,events,event_lease,owner,event,0),"stale setup failed");
    events.clear(); (void)events.declare("start");
    check(!queue.dispatch_first(queue_handle,references,events,dispatch),"cleared generation must be pruned");
    check(queue.size(queue_handle)==0 && calls==2,"stale event must not dispatch");
    auto fresh=events.lease();
    check(queue.enqueue(queue_handle,references,scene,events,fresh,owner,event,0),"old generation setup failed");
    events.clear(); (void)events.declare("start");
    auto next=events.lease();
    check(queue.enqueue(queue_handle,references,scene,events,next,owner,event,other) && queue.size(queue_handle)==1,
          "old event generation must not reserve an owner key");
    unsigned fresh_calls=0;
    check(queue.dispatch_first(queue_handle,references,events,[&](auto delivered_owner,auto delivered_event,auto argument) {
      check(delivered_owner==11 && delivered_event==event && argument==other,"new generation entry changed");
      ++fresh_calls;
    }) && fresh_calls==1,"new event generation must dispatch once");
    fresh=events.lease();
    check(queue.enqueue(queue_handle,references,scene,events,fresh,owner,event,0),"retired setup failed");
    references.retire();
    check(!queue.dispatch_first(queue_handle,references,events,dispatch),"retired generation must be pruned");
    check(queue.size(queue_handle)==0 && calls==2,"retired scene must not dispatch");
    DeferredSequenceStartQueue::Handle expired;
    { DeferredSequenceStartQueue temporary; expired=temporary.handle(); }
    check(!queue.live(expired),"destroyed queue handle stayed live");
  }
  static_assert(sizeof(off::runtime::SceneEventNames)<256,
                "Scene event reverse storage must not inflate each caller's stack frame");
  off::runtime::SceneEventNames names;
  const auto first_lease=names.lease();
  off::runtime::SceneEventNames foreign;
  check(names.is_live(first_lease) && !foreign.is_live(first_lease),"lease must bind to its event table");
  off::runtime::SceneEventNames::Lease destroyed;
  { off::runtime::SceneEventNames temporary; destroyed=temporary.lease(); }
  check(!names.is_live(destroyed),"destroyed event tables must not leave live leases");
  check(!names.initialized() && names.counter()==0 && !names.find("FadeIn") && !names.name(0),
        "fresh event state and read-only lookup must not initialize the registry");
  check(names.declare("Unrelated")==1 && names.initialized() && names.counter()==1 &&
        names.find("CAM_ENTERCAMERA")==0x401 && names.find("cam_leavecamera")==0x400,
        "lazy reserved declarations do not consume the dynamic sequence");
  check(names.declare("fAdEiN")==2 && names.declare("FadeIn",0xffff)==2 && names.counter()==2 &&
        names.name(2)=="fadein","ASCII canonical-name hits retain identity regardless of requested ID");
  check(names.declare("FadeOut")==3 && names.counter()==3,"absent fade name advances retained scene sequence");
  check(names.declare("Explicit",900)==900 && names.counter()==3,"explicit identities do not advance dynamic counter");
  rejects([&]{(void)names.declare("Collision",900);});
  rejects([&]{(void)names.declare("ReservedCollision",0x400);});
  rejects([&]{(void)names.declare("TooLarge",0x402);});
  rejects([&]{(void)names.declare(std::string("bad\0name",8));});
  check(!names.find("Collision") && !names.find("TooLarge") && names.counter()==3 && names.name(900)=="explicit",
        "rejected declarations preserve both indices and the dynamic counter");
  const std::string non_ascii="\xc3\x84";
  const auto unicode=names.declare(non_ascii+"A");
  check(names.name(unicode)==non_ascii+"a","canonicalization changes only ASCII A through Z");
  names.clear();
  check(!names.is_live(first_lease) && names.is_live(names.lease()),"clear must invalidate earlier event leases");
  check(names.declare("Unrelated")==1 && !names.is_live(first_lease),"reusing an event number must not revive an old lease");
  names.clear();
  check(!names.initialized() && names.counter()==0 && !names.name(0x400) && !names.find("FadeIn"),
        "explicit scene clear releases reserved and dynamic names");
  for(std::uint16_t i=1;i<0x400;++i)
    check(names.declare("Independent"+std::to_string(i))==i,"dynamic identifiers retain sequence order");
  rejects([&]{(void)names.declare("Exhausted");});
  check(names.counter()==0x3ff && !names.find("Exhausted") && names.name(0x400)=="cam_leavecamera",
        "dynamic counter cannot overwrite reserved reverse identities or wrap");
}
