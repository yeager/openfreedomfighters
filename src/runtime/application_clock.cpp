#include "off/runtime/application_clock.hpp"
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::runtime {
ClockSamplingServices make_monotonic_clock_samples() {
  const auto origin=std::chrono::steady_clock::now();
  return {
    [origin] { return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-origin).count(); },
    [origin] {
      const auto value=std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now()-origin).count();
      if(value<0 || value>std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("Native clock exceeded supported CRT sample domain");
      return static_cast<std::int32_t>(value);
    }
  };
}
namespace {
void validate(const ClockSamplingServices& s) {
  if(!s.alternate_counter || !s.crt_milliseconds)
    throw std::runtime_error("Application clock requires both sample services");
}
float rounded(double value) {
  if(!std::isfinite(value) || value>std::numeric_limits<float>::max() ||
     value<-std::numeric_limits<float>::max())
    throw std::runtime_error("Application clock float overflow");
  const float result=static_cast<float>(value);
  if(!std::isfinite(result)) throw std::runtime_error("Application clock float overflow");
  return result;
}
std::int32_t units(double value) {
  const double result=std::trunc(value*1024.0);
  if(!std::isfinite(result) || result<std::numeric_limits<std::int32_t>::min() ||
     result>std::numeric_limits<std::int32_t>::max())
    throw std::runtime_error("Application clock integer conversion overflow");
  return static_cast<std::int32_t>(result);
}
std::uint32_t word(std::int32_t value) { return std::bit_cast<std::uint32_t>(value); }
std::int32_t signed_word(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
}
ApplicationClock::ApplicationClock(ClockExecutionPolicy policy, ApplicationClockState state):state_(state) {
  if(policy!=ClockExecutionPolicy::no_recording_or_replay)
    throw std::runtime_error("Application timing recording/replay is unsupported");
  for(double value:{double(state.rate),state.unscaled_elapsed,state.prior_crt_sample,
      state.scaled_elapsed,state.smoothing,double(state.raw_delta),
      double(state.last_scaled_increment),double(state.scene_delta)})
    if(!std::isfinite(value)) throw std::runtime_error("Invalid retained application timing state");
}
void ApplicationClock::check_live() const {
  if(failed_) throw std::runtime_error("Application clock previously failed");
}
void ApplicationClock::assign_crt_mode(bool mode) { check_live(); state_.crt_mode=mode; }
void ApplicationClock::set_rate(float rate) {
  check_live();
  if(!std::isfinite(rate)) throw std::runtime_error("Application clock rate must be finite");
  state_.rate=rate;
}
void ApplicationClock::suppress_next_accumulation() { check_live(); state_.suppress_next=true; }
double ApplicationClock::sample_crt(const ClockSamplingServices& s) {
  const auto input=s.crt_milliseconds();
  if(input<0 || (last_crt_input_ && input<*last_crt_input_))
    throw std::runtime_error("Application clock CRT sample failed or moved backwards");
  last_crt_input_=input;
  return double(input)*0.001;
}
void ApplicationClock::sample_rebase(const ClockSamplingServices& s) {
  state_.alternate_counter=s.alternate_counter();
  state_.prior_crt_sample=double(rounded(sample_crt(s)));
}
void ApplicationClock::reset(const ClockSamplingServices& supplied) {
  check_live();
  if(running_) throw std::runtime_error("Reentrant application clock reset");
  const auto s=supplied; validate(s);
  running_=true;
  try {
    state_.unscaled_elapsed=state_.prior_crt_sample=state_.scaled_elapsed=state_.smoothing=0;
    state_.raw_integer=state_.previous_raw_integer=state_.scaled_integer=0;
    state_.scene_integer=state_.previous_scene_integer=state_.scene_offset=0;
    state_.raw_delta=0;
    sample_rebase(s);
    ready_=true; running_=false;
  } catch(...) { failed_=true; running_=false; throw; }
}
void ApplicationClock::rebase(const ClockSamplingServices& supplied) {
  check_live();
  if(running_) throw std::runtime_error("Reentrant application clock rebase");
  const auto s=supplied; validate(s);
  running_=true;
  try { sample_rebase(s); ready_=true; running_=false; }
  catch(...) { failed_=true; running_=false; throw; }
}
std::int32_t ApplicationClock::advance_crt(const ClockSamplingServices& supplied) {
  check_live();
  if(running_ || !ready_ || !state_.crt_mode)
    throw std::runtime_error("Application clock requires rebased CRT mode and no reentry");
  const auto s=supplied; validate(s);
  running_=true;
  try {
    const double now=sample_crt(s), old_prior=state_.prior_crt_sample;
    state_.prior_crt_sample=double(rounded(now));
    state_.previous_raw_integer=state_.raw_integer;
    state_.raw_delta=rounded(now-old_prior);
    state_.unscaled_elapsed+=double(state_.raw_delta);
    state_.raw_integer=units(state_.unscaled_elapsed);
    state_.alternate_counter=s.alternate_counter();
    if(!state_.crt_mode) throw std::runtime_error("Alternate clock mode became active during sampling");
    if(!state_.suppress_next) {
      state_.last_scaled_increment=rounded(double(state_.rate)*double(state_.raw_delta));
      state_.scaled_elapsed+=double(state_.last_scaled_increment);
      state_.scaled_integer=units(state_.scaled_elapsed);
    }
    state_.suppress_next=false;
    running_=false;
    return state_.scaled_integer;
  } catch(...) { failed_=true; running_=false; throw; }
}
void ApplicationClock::publish_scene(bool frozen) {
  check_live();
  if(running_ || !ready_) throw std::runtime_error("Scene publication requires completed clock sampling");
  state_.previous_scene_integer=state_.scene_integer;
  if(frozen) { state_.scene_delta=0; return; }
  state_.scene_integer=signed_word(word(state_.scaled_integer)+word(state_.scene_offset));
  const auto difference=signed_word(word(state_.scene_integer)-word(state_.previous_scene_integer));
  state_.scene_delta=static_cast<float>(difference)*(1.0F/1024.0F);
}
void ApplicationClock::apply_freeze_request(SceneFreezeState& freeze,bool requested,std::int32_t upstream) {
  check_live();
  if(running_ || !ready_) throw std::runtime_error("Scene freeze update requires completed clock sampling");
  freeze.requested=requested;
  if(freeze.current==requested) return;
  if(requested) freeze.snapshot=upstream;
  else state_.scene_offset=signed_word(word(state_.scene_offset)+word(freeze.snapshot)-word(upstream));
  freeze.current=requested;
}
std::uint32_t ApplicationClock::scene_integer_word() const {
  check_live();
  if(!ready_) throw std::runtime_error("Scene clock has not been rebased");
  return word(state_.scene_integer);
}
} // namespace off::runtime
