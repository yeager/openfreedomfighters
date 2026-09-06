#include "off/platform/sdl_intro_presentation.hpp"
#include "off/graphics/renderer_frame.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(std::string(message)+": "+SDL_GetError()); }
template<class F> void rejects(F action) { bool caught=false; try { action(); } catch(const std::runtime_error&) { caught=true; } require(caught,"expected rejection"); }
struct Session {
  SDL_Window* window{};
  SDL_GPUDevice* device{};
  bool claimed{};
  ~Session() {
    if(device) {
      SDL_WaitForGPUIdle(device);
      if(claimed) SDL_ReleaseWindowFromGPUDevice(device,window);
      SDL_DestroyGPUDevice(device);
    }
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
  }
};
}
int main() {
  using namespace off::graphics; using namespace off::platform;
  try {
    bool rejected=false;
    try { SdlIntroPresentation invalid(nullptr,nullptr,0,SDL_GPU_TEXTUREFORMAT_INVALID); }
    catch(const std::runtime_error&) { rejected=true; }
    require(rejected,"reject missing device/window");
    Session session;
    if(!SDL_Init(SDL_INIT_VIDEO)) { std::cout<<"SKIP: video unavailable\n"; return 77; }
    // SDL window/swapchain operations are required, including when the video
    // driver is offscreen. That driver does not prove visible desktop output.
    session.window=SDL_CreateWindow("OpenFreedomFighters phase-two presentation test",64,48,SDL_WINDOW_HIDDEN);
    const char* driver=SDL_getenv("OFF_PICTURE_SHADER_TEST_DRIVER");
    session.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL,false,driver&&*driver?driver:nullptr);
    if(!session.window||!session.device||!SDL_ClaimWindowForGPUDevice(session.device,session.window)) {
      std::cout<<"SKIP: actual window swapchain unavailable: "<<SDL_GetError()<<'\n'; return 77;
    }
    session.claimed=true;
    // Some platforms do not produce swapchain images for hidden windows.
    require(SDL_ShowWindow(session.window),"show presentation test window"); SDL_PumpEvents();
    auto depth_format=SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    if(!SDL_GPUTextureSupportsFormat(session.device,depth_format,SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
      depth_format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    if(!SDL_GPUTextureSupportsFormat(session.device,depth_format,SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
      std::cout<<"SKIP: neither supported stencil attachment format is available\n"; return 77;
    }
    SdlIntroPresentation adapter(session.device,session.window,7,depth_format);
    IntroControllerPhaseTwoServices services;
    unsigned registrations=0,mode_assignments=0,lookups=0;
    services.input_manager_exists=[] { return true; };
    services.register_movie_control_action_map=[&] { ++registrations; };
    services.assign_engine_clock_mode=[&](bool value) { require(value,"true engine clock mode"); ++mode_assignments; };
    services.query_global_property=[](std::string_view,std::uint32_t& value) { require(value==0,"zero output on absent synthetic global properties"); };
    services.current_audio_volume=[] { return 50; };
    services.request_audio_volume=[](std::uint32_t) { throw std::runtime_error("valid volume must not be changed"); };
    services.scene_integer_clock=[] { return 1024U; };
    services.first_renderer=[&] { ++lookups; return 7U; };
    adapter.bind_renderer_services(services);
    rejects([&] { (void)services.renderer_width(999); });
    rejects([&] { services.set_viewport(7,{0,0,1,1,0,1}); });
    require(!adapter.failed(),"query/viewport preflight rejection leaves adapter usable");
    IntroControllerInitialization controller;
    RendererFrameClock frame_clock;
    controller.run_phase_two(services);
    require(controller.phase_two_completed()&&controller.deadline()==3072,"phase-two callback completed with real presentation tail");
    require(adapter.completed_presentations()==2&&!adapter.failed()&&lookups==5,"two actual SDL submissions/presentations with five renderer lookups");
    require(registrations==1&&mode_assignments==1&&frame_clock.value()==1,"non-renderer services preserved and no frame traversal");
    // A second clear before presentation is a protocol error. Its pending work
    // remains cancellable because no swapchain has yet been acquired.
    services.clear(7,{true,true,true,0,1,0});
    require(adapter.completed_presentations()==2,"clear alone does not present");
    rejects([&] { services.clear(7,{true,true,true,0,1,0}); });
    require(adapter.failed()&&adapter.completed_presentations()==2,"second clear cancels pending work and poisons without presentation");
    rejects([&] { (void)services.present(7); });
    {
      SdlIntroPresentation pending(session.device,session.window,8,depth_format);
      IntroControllerPhaseTwoServices tail;
      pending.bind_renderer_services(tail);
      tail.set_viewport(8,{0,0,static_cast<Uint32>(tail.renderer_width(8)),static_cast<Uint32>(tail.renderer_height(8)),0,1});
      tail.clear(8,{true,true,true,0,1,0});
      require(pending.completed_presentations()==0,"destruction cancels an unpresented clear");
    }
    std::cout<<"SDL video backend: "<<SDL_GetCurrentVideoDriver()
             <<"; GPU presentation backend: "<<SDL_GetGPUDeviceDriver(session.device)
             <<"; two clear/present submissions completed.\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
