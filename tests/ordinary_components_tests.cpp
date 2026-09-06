#include "off/runtime/ordinary_components.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
using namespace off::runtime;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F operation){bool caught=false;try{operation();}catch(const std::runtime_error&){caught=true;}check(caught,"expected ordinary manager rejection");}
std::vector<std::uint64_t> copy(std::span<const std::uint64_t> input){return {input.begin(),input.end()};}
void sort_case(std::vector<OrdinarySortItem> items,std::vector<std::uint64_t> expected,std::uint32_t state){
  OrdinarySortingState sorting;sorting.sort(items);std::vector<std::uint64_t> actual;
  for(auto item:items)actual.push_back(item.handle);
  check(actual==expected && sorting.state==state,"approved sorting collision/state fixture");
}
struct Fixture {
  SceneComponentSequence sequence{[]{return std::uint32_t{0};}};
  ComponentLifecycle lifecycle{sequence};
  std::set<std::uint64_t> stale;
  std::vector<std::string> effects;
  OrdinarySortingState sorting;
  OrdinaryComponentManager manager{sorting,{
    [&](std::uint64_t h)->ComponentRecord* {return h && h<=lifecycle.size() && !stale.contains(h)?&lifecycle.at(static_cast<std::size_t>(h-1)):nullptr;},
    [](std::uint64_t h)->std::optional<OrdinaryOwner>{return h?std::optional{OrdinaryOwner{h,std::nullopt}}:std::nullopt;},
    [](std::uint32_t h)->std::optional<OrdinaryOwner>{return h==99?std::optional{OrdinaryOwner{h,3}}:std::nullopt;}}};
  std::uint64_t add(std::uint32_t priority=0,std::uint16_t ordinal=1,std::uint32_t requested=0x11){
    const auto index=lifecycle.append({100+lifecycle.size(),std::nullopt,lifecycle.size(),"Synthetic",0,0,0,true});
    lifecycle.construct(index,[&](ComponentRecord& r){
      return ConstructedComponent{{ordinal,priority,requested,0x10,0,4,0,r.source().owner},[](auto&){},[](auto&){}};
    });return index+1;
  }
  ComponentState& state(std::uint64_t h){return lifecycle.at(static_cast<std::size_t>(h-1)).state();}
  OrdinaryDispatchServices dispatch(){return {
    [&]{effects.push_back("clock");return 77U;},
    [&](std::uint32_t t){check(t==77,"dispatch alias exact scene integer");effects.push_back("alias");},
    [&]{effects.push_back("pause");return false;},
    [&]{effects.push_back("filter");return std::optional<std::uint64_t>{};},
    [&](ComponentRecord& r){effects.push_back("event"+std::to_string(*r.identity()));},
    [&](OrdinaryOwner,ComponentRecord&){effects.push_back("owner");},
    [&](ComponentRecord&){effects.push_back("phase");},
    [&](ComponentRecord&,std::uint64_t){effects.push_back("retire");},false,{},{}};}
};
}
int main(){try{
  sort_case({{0,7},{1,7}},{1,0},0x83f78dd6U);
  sort_case({{0,7},{1,7},{2,7},{3,7}},{2,3,0,1},0xc932147aU);
  sort_case({{0,3},{1,1},{2,2},{3,1}},{1,3,2,0},0xf9988bbcU);
  {
    OrdinarySortingState sorting;std::array<OrdinarySortItem,1> one{{{1,0}}};sorting.sort(one);check(sorting.state==0x026065caU,"singleton does not advance sorting");
    std::array<OrdinarySortItem,2> pair{{{1,0},{2,0}}};sorting.sort(pair);const auto previous=sorting.state;
    sorting.sort(pair);check(sorting.state!=previous,"sorting state continues between consumers");
  }
  {
    Fixture f;const auto a=f.add(100),b=f.add(0),c=f.add(0,0);
    f.manager.enqueue(a);f.manager.enqueue(b);f.manager.enqueue(c);f.manager.enqueue(b);f.manager.enqueue(99);
    f.manager.refresh();check(copy(f.manager.retained())==std::vector<std::uint64_t>{c,b,a} && f.manager.pending().empty(),"two-stage priority/tie merge and pending duplicate suppression");
    check((f.state(b).registered_cache&0x10)!=0,"insertion publishes separate cache");
    const auto seed=f.sorting.state;f.manager.enqueue(b);f.manager.enqueue(c);f.manager.refresh();
    check(f.sorting.state!=seed && f.manager.retained().size()==3,"cached pending still consumes sorting state");
    f.state(b).admitted=0;f.state(c).admitted=0;f.state(c).registered_cache=0;
    f.manager.refresh();check(copy(f.manager.retained())==std::vector<std::uint64_t>{c,a} && f.state(b).registered_cache==0,"compaction retains both-bits-clear but clears cached inactive");
    f.stale.insert(a);f.manager.refresh();check(copy(f.manager.retained())==std::vector<std::uint64_t>{c},"stale retained removal");
  }
  {
    Fixture f;const auto a=f.add();f.manager.enqueue(a);f.manager.refresh();f.state(a).admitted=0;
    for(int i=0;i<200;++i)f.manager.notify_removal();
    check(f.manager.retained().size()==1,"200 notifications defer immediate compaction");
    f.manager.notify_removal();check(f.manager.retained().empty() && f.manager.removal_count()==0,"201 inactive notifications compact and reset");
  }
  {
    Fixture f;const auto a=f.add(),b=f.add();f.manager.enqueue(a);f.manager.enqueue(b);
    auto services=f.dispatch();services.direct_event16=[&](ComponentRecord& r){
      f.effects.push_back("event"+std::to_string(*r.identity()));
      if(*r.identity()==0){f.state(b).admitted=0;const auto c=f.add();f.manager.enqueue(c);f.manager.notify_removal();}
    };
    f.manager.dispatch(services);
    check(f.effects==std::vector<std::string>{"clock","alias","pause","filter","event0"} && f.manager.pending().size()==1,"clock/captured controls precede events; live admission changes skip later entry; new additions defer");
    f.effects.clear();f.manager.dispatch(f.dispatch());
    check(f.effects==std::vector<std::string>{"clock","alias","pause","filter","event0","event2"},"next refresh admits callback addition");
  }
  {
    Fixture f;const auto a=f.add(),b=f.add(0,2,0x111);f.manager.enqueue(a);f.manager.enqueue(b);
    auto services=f.dispatch();services.paused=[]{return true;};services.filter=[&]{return std::optional{b};};
    f.manager.dispatch(services);check(f.effects==std::vector<std::string>{"clock","alias","event1"},"captured pause bypass and filter");
    f.effects.clear();services.paused=[]{return false;};f.state(b).status=0;
    f.manager.dispatch(services);check(f.effects==std::vector<std::string>{"clock","alias","phase"},"phase-one gate diagnoses instead of event16");
  }
  {
    Fixture f;const auto a=f.add();f.manager.enqueue(a);auto services=f.dispatch();
    services.profiling=true;services.profile_begin=[&](auto&){f.effects.push_back("begin");};services.profile_end=[&](auto&){f.effects.push_back("end");};
    services.direct_event16=[&](auto& r){f.effects.push_back("event");r.state().status|=1;r.state().attached_owner=222;};
    services.retire=[&](auto&,auto captured){check(captured==100,"retirement uses precallback owner");f.effects.push_back("retire");};
    f.manager.dispatch(services);check(f.effects==std::vector<std::string>{"clock","alias","pause","filter","begin","event","end","retire"},"retired prestatus does not bypass callback and tail uses live owner");
    f.effects.clear();services.direct_event16=[&](auto& r){f.effects.push_back("event");r.state().attached_owner=0;};
    f.manager.dispatch(services);check(f.effects.back()=="event","null postcallback attached owner skips profiling/retirement tail");
  }
  {
    Fixture f;const auto a=f.add();f.state(a).script_reference=99;f.state(a).attached_owner=0;
    f.manager.enqueue(a);auto services=f.dispatch();services.direct_event16={};f.manager.dispatch(services);
    check(f.effects.back()=="owner","nonzero reference uses actual owner route not event16");
  }
  {
    Fixture f;const auto a=f.add();f.manager.enqueue(a);auto services=f.dispatch();
    services.direct_event16=[&](auto&){f.manager.refresh();};rejects([&]{f.manager.dispatch(services);});
    check(f.manager.failed() && !f.manager.traversing(),"uncaught callback reentry poisons completed prefix");
  }
  {
    Fixture f;const auto a=f.add(0xffffffffU);f.manager.enqueue(a);rejects([&]{f.manager.refresh();});check(f.manager.failed(),"sentinel priority explicitly rejected");
  }
  {
    Fixture f;const auto a=f.add();for(std::size_t i=0;i<600;++i)f.manager.enqueue(a);
    f.manager.enqueue(a);check(f.manager.retained().size()==1 && f.manager.pending().size()==1,"idle pending overflow refreshes before append");
  }
  {
    Fixture f;const auto a=f.add(),b=f.add();f.manager.enqueue(a);f.manager.enqueue(b);f.manager.refresh();
    const auto c=f.add(),d=f.add();const auto before=f.sorting.state;
    OrdinaryComponentManager second(f.sorting,{
      [&](std::uint64_t h){return &f.lifecycle.at(static_cast<std::size_t>(h-1));},{},{}});
    second.enqueue(c);second.enqueue(d);second.refresh();
    auto expected=before;for(int i=0;i<2;++i)expected=expected+std::rotl(expected,int(expected&31U))+3;
    check(f.sorting.state==expected && copy(second.retained())==std::vector<std::uint64_t>{c,d},"separate managers advance same generator through both sorting stages");
  }
  {
    Fixture f;const auto a=f.add();f.manager.enqueue(a);f.manager.refresh();f.state(a).priority=1;
    rejects([&]{f.manager.refresh();});check(f.manager.failed(),"unsupported retained key mutation cannot silently break ordering");
  }
  {
    Fixture f;const auto a=f.add();f.manager.enqueue(a);auto services=f.dispatch();
    services.direct_event16=[&](auto&){for(std::size_t i=0;i<601;++i)f.manager.enqueue(a);};
    rejects([&]{f.manager.dispatch(services);});
    check(f.manager.failed() && f.manager.pending().size()==600,"traversal pending overflow preserves bounded completed prefix");
  }
  {
    Fixture f;
    for(std::size_t i=0;i<1200;++i)f.manager.enqueue(f.add());
    f.manager.refresh();check(f.manager.retained().size()==1200,"retained native capacity admitted");
    f.manager.enqueue(f.add());rejects([&]{f.manager.refresh();});
    check(f.manager.failed() && f.manager.retained().size()==1200,"retained overflow never publishes oversized array");
  }
  std::cout<<"Ordinary sorting, membership, cache, live dispatch and service failure boundaries verified.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
