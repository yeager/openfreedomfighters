#include "off/graphics/intro_controller_initialization.hpp"
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
} // namespace off::graphics
