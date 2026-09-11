#include "off/graphics/intro_controller_initialization.hpp"
#include <bit>
#include <cctype>
#include <stdexcept>

namespace off::graphics {
void IntroControllerInitialization::validate_services(const IntroControllerPhaseTwoServices& s) {
  if(!s.input_manager_exists||!s.register_movie_control_action_map||!s.assign_engine_clock_mode||
     !s.query_global_property||!s.current_audio_volume||!s.request_audio_volume||!s.scene_integer_clock||
     !s.first_renderer||!s.renderer_height||!s.renderer_width||!s.set_viewport||!s.renderer_has_stencil||!s.clear||!s.present)
    throw std::runtime_error("intro phase two requires every named service");
}

void IntroControllerInitialization::run_phase_two(const IntroControllerPhaseTwoServices& supplied) {
  if(running_||failed_) throw std::runtime_error("intro phase two is reentrant or previously failed");
  // Snapshot function objects before effects; callbacks may replace the caller's
  // service table without invalidating the currently executing callable.
  const auto s=supplied;
  validate_services(s);
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

namespace {
[[nodiscard]] bool equal_case_insensitive(std::string_view left,std::string_view right) {
  if(left.size()!=right.size()) return false;
  for(std::size_t index=0;index<left.size();++index) {
    const auto a=static_cast<unsigned char>(left[index]);
    const auto b=static_cast<unsigned char>(right[index]);
    if(std::tolower(a)!=std::tolower(b)) return false;
  }
  return true;
}
[[nodiscard]] std::uint32_t little_endian_word(const std::vector<std::uint8_t>& bytes) {
  if(bytes.size()!=4) throw std::runtime_error("MainCamera property must be exactly four bytes");
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1])<<8U) |
         (static_cast<std::uint32_t>(bytes[2])<<16U) |
         (static_cast<std::uint32_t>(bytes[3])<<24U);
}
}

FirstCutRequestedCameraRoute::FirstCutRequestedCameraRoute(bool first_option,
    bool optional_renderer_flag,std::uint64_t requested_reference)
  :first_option_(first_option),optional_renderer_flag_(optional_renderer_flag),
   requested_reference_(requested_reference) {
  if(!requested_reference_) throw std::runtime_error("first cut requires a live requested camera reference");
}

FirstCutRequestedCameraResult FirstCutRequestedCameraRoute::run(
    const FirstCutRequestedCameraServices& supplied) {
  if(running_||failed_) throw std::runtime_error("first-cut requested-camera route is reentrant or previously failed");
  const auto s=supplied;
  if(!s.read_scene_property||!s.resolve_requested_camera)
    throw std::runtime_error("first-cut requested-camera route requires property and requested-reference services");
  running_=true;
  same_update_visible_=false;
  try {
    std::uint32_t main_camera=0;
    if(const auto property=s.read_scene_property("MainCamera");property)
      main_camera=little_endian_word(*property);
    const auto requested=s.resolve_requested_camera(requested_reference_);
    if(!requested || !requested->reference || !requested->runtime_owner)
      throw std::runtime_error("first-cut requested camera resolution failed");
    if(first_option_ && main_camera!=0) {
      if(!s.resolve_named_camera||!s.run_named_camera_route)
        throw std::runtime_error("nonzero MainCamera requires the separate named-camera service");
      if(!s.resolve_named_camera(main_camera))
        throw std::runtime_error("nonzero MainCamera named-camera resolution failed");
      s.run_named_camera_route(main_camera,*requested);
      same_update_visible_=true;
      running_=false;
      return FirstCutRequestedCameraResult::named_camera_route;
    }
    if(first_option_ && main_camera==0) {
      if(!s.increment_renderer_control||!s.assign_renderer_optional_flag||
         !s.visit_registered_cameras_live||!s.resolve_registered_camera||
         !s.current_scene_identifier||!s.disable_camera)
        throw std::runtime_error("zero MainCamera sweep requires every renderer service");
      if(!selected_reference_) {
        s.increment_renderer_control();
        s.assign_renderer_optional_flag(optional_renderer_flag_);
      }
      const auto scene=s.current_scene_identifier();
      s.visit_registered_cameras_live([&](const FirstCutRegisteredCamera& entry) {
        const auto owner=s.resolve_registered_camera(entry.runtime_owner);
        if(!owner) return;
        const bool preserve=equal_case_insensitive(owner->name,"FFTVOSDCam") ||
          equal_case_insensitive(owner->name,"FFTVNoiseCam") ||
          (equal_case_insensitive(owner->name,"ZWindowsCamera") &&
           scene.find("Eidos_IntroController")!=std::string::npos);
        if(!preserve) s.disable_camera(owner->runtime_owner);
      });
    }
    if(!s.assign_scene_root_background)
      throw std::runtime_error("requested-camera continuation requires scene background service");
    s.assign_scene_root_background(requested->packed_background);
    if(requested->camera_class) {
      if(!s.register_first_renderer_camera||!s.notify_renderer_dimensions)
        throw std::runtime_error("camera-class requested continuation requires renderer services");
      static_cast<void>(s.register_first_renderer_camera(requested->runtime_owner,
          static_cast<float>(requested->priority)));
      s.notify_renderer_dimensions(requested->runtime_owner);
      if(s.notify_audio_camera) s.notify_audio_camera(requested->runtime_owner);
    }
    selected_reference_=requested->reference;
    same_update_visible_=true;
    running_=false;
    return FirstCutRequestedCameraResult::requested_camera_selected;
  } catch(...) {
    running_=false;
    failed_=true;
    throw;
  }
}
} // namespace off::graphics
