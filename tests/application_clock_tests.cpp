#include "off/runtime/application_clock.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace off::runtime;
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
  bool rejected=false; try { f(); } catch(const std::runtime_error&) { rejected=true; }
  check(rejected,"expected rejection");
}
}
int main() {
  try {
    std::int32_t sample=1000;
    std::int64_t counter=0;
    std::vector<std::string> calls;
    ClockSamplingServices source{[&] { calls.push_back("counter"); return ++counter; },
                                [&] { calls.push_back("crt"); return sample; }};
    ApplicationClock clock(ClockExecutionPolicy::no_recording_or_replay);
    check(clock.state().rate==1&&!clock.state().crt_mode&&!clock.ready(),"fresh rate/mode are not sampled time");
    rejects([&] { clock.advance_crt(source); });
    clock.reset(source);
    check(calls==std::vector<std::string>{"counter","crt"}&&clock.state().prior_crt_sample==1,"reset rebase order and sample");
    rejects([&] { clock.advance_crt(source); });
    check(calls.size()==2,"unsupported alternate mode rejects before samples");
    clock.assign_crt_mode(true); clock.set_rate(2);
    check(calls.size()==2&&clock.state().prior_crt_sample==1,"mode/rate assignment does not sample or reset");
    calls.clear(); sample=1250;
    check(clock.advance_crt(source)==512,"scaled integer units");
    clock.publish_scene(false);
    check(calls==std::vector<std::string>{"crt","counter"}&&clock.state().raw_integer==256&&
          clock.state().previous_raw_integer==0&&clock.state().last_scaled_increment==0.5F&&clock.state().scene_delta==0.5F,
          "CRT producer stores raw history then samples counter before scaling");
    SceneFreezeState freeze{false,false,0};
    clock.apply_freeze_request(freeze,true,clock.state().scaled_integer);
    sample=1500; clock.advance_crt(source); clock.publish_scene(freeze.current);
    check(clock.state().scene_integer==512&&clock.state().scene_delta==0&&freeze.snapshot==512,"frozen scene keeps time while producer advances");
    clock.apply_freeze_request(freeze,false,clock.state().scaled_integer);
    sample=1750; clock.advance_crt(source); clock.publish_scene(freeze.current);
    check(clock.state().scene_integer==1024&&clock.state().scene_offset==-512&&clock.state().scene_delta==0.5F,"resume compensates upstream frozen interval");
    clock.suppress_next_accumulation(); sample=2000; clock.advance_crt(source); clock.publish_scene(false);
    check(clock.state().raw_integer==1024&&clock.state().previous_raw_integer==768&&clock.state().scaled_integer==1536&&
          clock.state().last_scaled_increment==0.5F&&!clock.state().suppress_next&&clock.state().scene_delta==0,
          "suppression retains last scaled increment but still samples raw/counter time");
    sample=2250; clock.advance_crt(source); clock.publish_scene(false);
    const auto before=clock.state();
    clock.rebase(source);
    check(clock.state().scaled_elapsed==before.scaled_elapsed&&clock.state().scene_integer==before.scene_integer,
          "focus-style rebase preserves accumulated/published time");
    clock.suppress_next_accumulation();
    const auto retained_delta=clock.state().scene_delta;
    clock.reset(source);
    check(clock.state().unscaled_elapsed==0&&clock.state().scaled_elapsed==0&&clock.state().scene_integer==0&&
          clock.state().scene_offset==0&&clock.state().raw_delta==0&&clock.state().scene_delta==retained_delta&&
          clock.state().last_scaled_increment==0.5F&&clock.state().rate==2&&clock.state().crt_mode&&clock.state().suppress_next&&
          freeze.snapshot==512&&!freeze.current&&!freeze.requested,"full reset clears only specified timing fields, never external freeze state");
    ApplicationClock rounding(ClockExecutionPolicy::no_recording_or_replay);
    sample=1001; rounding.rebase(source); rounding.assign_crt_mode(true); rounding.advance_crt(source);
    const double now=double(sample)*0.001;
    check(rounding.state().raw_delta==static_cast<float>(now-double(static_cast<float>(now)))&&rounding.state().raw_delta!=0,
          "saved prior sample has binary32 rounding even for repeated equal input");
    ApplicationClock failure(ClockExecutionPolicy::no_recording_or_replay);
    sample=1000; failure.reset(source); failure.assign_crt_mode(true); sample=1250;
    auto broken=source;
    broken.alternate_counter=[&]()->std::int64_t {
      check(failure.state().raw_integer==256&&failure.state().previous_raw_integer==0,"counter sees already-published raw time");
      throw std::runtime_error("counter unavailable");
    };
    rejects([&] { failure.advance_crt(broken); });
    check(failure.failed()&&failure.state().raw_integer==256&&failure.state().scaled_integer==0,"sample failure preserves raw prefix without fake scaled progress");
    rejects([&] { failure.advance_crt(source); });
    ApplicationClock backwards(ClockExecutionPolicy::no_recording_or_replay);
    backwards.reset(source); backwards.assign_crt_mode(true); sample=1;
    rejects([&] { backwards.advance_crt(source); });
    ApplicationClock overflow(ClockExecutionPolicy::no_recording_or_replay);
    sample=0; overflow.reset(source); overflow.assign_crt_mode(true); sample=std::numeric_limits<std::int32_t>::max();
    rejects([&] { overflow.advance_crt(source); });
    ApplicationClock float_overflow(ClockExecutionPolicy::no_recording_or_replay);
    sample=0; float_overflow.reset(source); float_overflow.assign_crt_mode(true);
    float_overflow.set_rate(std::numeric_limits<float>::max()); sample=2000;
    rejects([&] { float_overflow.advance_crt(source); });
    check(float_overflow.failed()&&float_overflow.state().raw_integer==2048&&
          float_overflow.state().last_scaled_increment==0&&float_overflow.state().scaled_integer==0,
          "out-of-range double product rejects before conversion/publication of float increment");
    ApplicationClock missing(ClockExecutionPolicy::no_recording_or_replay);
    rejects([&] { missing.reset({}); });
    check(!missing.failed()&&!missing.ready(),"missing services preflight has no effects");
    const auto native=make_monotonic_clock_samples();
    check(native.crt_milliseconds()>=0&&native.alternate_counter()>=0,"native monotonic adapter returns supported samples");
    std::cout<<"Canonical CRT clock, reset/rebase, freeze and failure boundaries verified.\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
