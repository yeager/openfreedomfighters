#include "off/graphics/intro_controller_initialization.hpp"
#include <bit>
#include <stdexcept>

namespace off::graphics {
void IntroControllerInitialization::run_phase_two(const IntroControllerPhaseTwoServices& supplied) {
  if(running_||failed_) throw std::runtime_error("intro phase two is reentrant or previously failed");
  // Snapshot function objects before effects; callbacks may replace the caller's
  // service table without invalidating the currently executing callable.
  const auto s=supplied;
  if(!s.input_manager_exists||!s.register_movie_control_action_map||!s.assign_engine_clock_mode||
     !s.query_global_property||!s.current_audio_volume||!s.request_audio_volume||!s.scene_integer_clock||
     !s.first_renderer||!s.renderer_height||!s.renderer_width||!s.set_viewport||!s.renderer_has_stencil||!s.clear||!s.present)
    throw std::runtime_error("intro phase two requires every named service");
  running_=true; completed_=false;
  try {
    if(s.input_manager_exists()) s.register_movie_control_action_map();
    s.assign_engine_clock_mode(true);
    mode_assigned_=true;
    std::uint32_t memory_audio=0;
    s.query_global_property("SoundReadFromMem",memory_audio);
    if(memory_audio!=0) {
      std::uint32_t volume=0;
      s.query_global_property("SfxV",volume);
      s.request_audio_volume(volume<=100?volume:100);
    } else {
      const auto volume=s.current_audio_volume();
      if(volume<0||volume>100) s.request_audio_volume(100);
    }
    deadline_=s.scene_integer_clock()+std::uint32_t{2048};
    deadline_assigned_=true;
    const auto renderer=[&] {
      const auto id=s.first_renderer();
      if(id==0) throw std::runtime_error("intro phase two requires a live first renderer");
      return id;
    };
    const auto viewport_renderer=renderer();
    const auto height=s.renderer_height(viewport_renderer);
    const auto width=s.renderer_width(viewport_renderer);
    // Exact positive float-representable integer dimensions are the supported
    // native conversion boundary; unusual original float-to-int cases reject.
    if(height<=0||width<=0||height>0x1000000||width>0x1000000)
      throw std::runtime_error("intro phase two renderer dimensions are unsupported");
    const PictureDeviceViewport viewport{0,0,static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),0,1};
    s.set_viewport(viewport_renderer,viewport);
    for(unsigned repetition=0;repetition<2;++repetition) {
      const auto clear_renderer=renderer();
      PictureViewClear request{};
      request.color=true; request.depth=true;
      request.stencil=s.renderer_has_stencil(clear_renderer);
      request.packed_color=0; request.depth_value=1; request.stencil_value=0;
      s.clear(clear_renderer,request);
      if(s.present(renderer())!=IntroPresentationResult::presented)
        throw std::runtime_error("intro phase two presentation recovery is unsupported");
    }
    completed_=true; running_=false;
  } catch(...) { running_=false; failed_=true; throw; }
}

namespace {
[[nodiscard]] std::int32_t wrapping_clock_add(std::int32_t left,std::int32_t right) noexcept {
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left)+static_cast<std::uint32_t>(right));
}
}

MovieControlFirstUpdate::MovieControlFirstUpdate(std::uint64_t component_handle,
    std::uint64_t owner_handle,std::int32_t delay)
  :component_handle_(component_handle),owner_handle_(owner_handle),delay_(delay) {
  if(!component_handle_ || !owner_handle_)
    throw std::runtime_error("MovieControl requires live component and owner handles");
}

void MovieControlFirstUpdate::run_phase_two(const MovieControlPhaseTwoServices& supplied) {
  if(phase_two_running_ || event_running_ || failed_)
    throw std::runtime_error("MovieControl phase two is reentrant or previously failed");
  const auto services=supplied;
  if(!services.input_manager_exists || !services.register_action_map ||
      !services.assign_engine_clock_mode || !services.setup_audio_volume ||
      !services.scene_integer_clock || !services.setup_and_clear_first_renderer)
    throw std::runtime_error("MovieControl phase two requires every named service");
  phase_two_running_=true;
  phase_two_returned_=false;
  try {
    if(services.input_manager_exists()) services.register_action_map();
    services.assign_engine_clock_mode(true);
    services.setup_audio_volume();
    deadline_=wrapping_clock_add(services.scene_integer_clock(),delay_);
    deadline_assigned_=true;
    services.setup_and_clear_first_renderer();
    phase_two_returned_=true;
    phase_two_running_=false;
  } catch(...) {
    phase_two_running_=false;
    failed_=true;
    throw;
  }
}

MovieControlEvent16Result MovieControlFirstUpdate::dispatch_event16(
    const MovieControlEvent16Services& supplied) {
  if(phase_two_running_ || event_running_ || failed_)
    throw std::runtime_error("MovieControl event16 is reentrant or previously failed");
  const auto services=supplied;
  if(!services.component_is_live || !services.event16_enrolled || !services.paused ||
      !services.captured_component_filter || !services.phase_one_completed)
    throw std::runtime_error("MovieControl event16 requires manager admission services");
  if(!services.component_is_live()) return MovieControlEvent16Result::skipped_not_live;
  if(!services.event16_enrolled()) return MovieControlEvent16Result::skipped_not_enrolled;
  if(services.paused()) return MovieControlEvent16Result::skipped_paused;
  if(const auto filter=services.captured_component_filter();filter && *filter!=component_handle_)
    return MovieControlEvent16Result::skipped_filtered;
  if(!services.phase_one_completed()) return MovieControlEvent16Result::phase_one_incomplete;
  if(activated_) return MovieControlEvent16Result::already_active;
  if(!deadline_assigned_ || !services.scene_integer_clock || !services.prepare_sequence_resources ||
      !services.send_cut_sequence_start || !services.send_group_state_requests)
    throw std::runtime_error("MovieControl admitted event16 requires deadline and direct services");
  event_running_=true;
  try {
    const auto now=services.scene_integer_clock();
    if(now<=deadline_) {event_running_=false;return MovieControlEvent16Result::waiting_for_deadline;}
    services.prepare_sequence_resources();
    activated_=true;
    playback_baseline_=now;
    services.send_cut_sequence_start(owner_handle_);
    services.send_group_state_requests(owner_handle_);
    event_running_=false;
    return MovieControlEvent16Result::activated;
  } catch(...) {
    event_running_=false;
    failed_=true;
    throw;
  }
}
} // namespace off::graphics
