#include "off/platform/sdl_intro_presentation.hpp"
#include "off/platform/sdl_picture_clear.hpp"
#include <stdexcept>
#include <string>

namespace off::platform {
namespace {
[[noreturn]] void fail(const char* text) { throw std::runtime_error(std::string(text)+": "+SDL_GetError()); }
bool stencil(SDL_GPUTextureFormat f) { return f==SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT||f==SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT; }
}
struct SdlIntroPresentation::Impl {
  SDL_GPUDevice* device;
  SDL_Window* window;
  std::uint64_t identity;
  SDL_GPUTextureFormat color_format,depth_format;
  int width{},height{};
  std::unique_ptr<SdlPictureClear> clearer;
  SDL_GPUTexture *color{},*depth{};
  SDL_GPUCommandBuffer* command{};
  graphics::PictureDeviceViewport viewport{};
  bool viewport_set{},failed{},acquired{};
  std::uint64_t presentations{};
  Impl(SDL_GPUDevice* d,SDL_Window* w,std::uint64_t id,SDL_GPUTextureFormat df)
    :device(d),window(w),identity(id),color_format(SDL_GetGPUSwapchainTextureFormat(d,w)),depth_format(df) {}
  ~Impl() {
    abandon();
    SDL_WaitForGPUIdle(device);
    clearer.reset();
    if(depth) SDL_ReleaseGPUTexture(device,depth);
    if(color) SDL_ReleaseGPUTexture(device,color);
  }
  void abandon() noexcept {
    if(command) {
      auto* pending=command; command=nullptr;
      if(acquired) { SDL_SubmitGPUCommandBuffer(pending); SDL_WaitForGPUIdle(device); }
      else SDL_CancelGPUCommandBuffer(pending);
    }
    acquired=false;
  }
  void check(std::uint64_t id) {
    if(failed||id!=identity) throw std::runtime_error("intro presentation identity is not live or adapter failed");
    int w{},h{};
    if(!SDL_GetWindowSizeInPixels(window,&w,&h)) fail("query intro presentation dimensions");
    if(w!=width||h!=height||SDL_GetGPUSwapchainTextureFormat(device,window)!=color_format)
      throw std::runtime_error("intro presentation resize or configuration change is unsupported");
  }
  void clear(std::uint64_t id,const graphics::PictureViewClear& request) {
    try {
      check(id);
      if(command||!viewport_set||!request.color||!request.depth||request.stencil!=stencil(depth_format)||
         request.packed_color!=0||request.depth_value!=1||request.stencil_value!=0)
        throw std::runtime_error("intro phase-two clear requires explicit viewport and color/depth/stencil request");
      command=SDL_AcquireGPUCommandBuffer(device); if(!command) fail("acquire intro presentation command");
      clearer->encode(command,{color,color_format,depth,depth_format,static_cast<Uint32>(width),static_cast<Uint32>(height)},viewport,request);
    } catch(...) { failed=true; abandon(); throw; }
  }
  graphics::IntroPresentationResult present(std::uint64_t id) {
    try {
      check(id);
      if(!command) throw std::runtime_error("intro presentation requires a pending clear");
      SDL_GPUTexture* swapchain{}; Uint32 w{},h{};
      if(!SDL_WaitAndAcquireGPUSwapchainTexture(command,window,&swapchain,&w,&h)) fail("acquire intro presentation swapchain");
      acquired=swapchain!=nullptr;
      if(!swapchain) throw std::runtime_error("intro presentation has no swapchain image (possibly minimized)");
      if(w!=static_cast<Uint32>(width)||h!=static_cast<Uint32>(height))
        throw std::runtime_error("intro presentation swapchain resized after acquisition");
      SDL_GPUBlitInfo blit{};
      blit.source.texture=color; blit.source.w=w; blit.source.h=h;
      blit.destination.texture=swapchain; blit.destination.w=w; blit.destination.h=h;
      blit.load_op=SDL_GPU_LOADOP_DONT_CARE; blit.filter=SDL_GPU_FILTER_NEAREST;
      SDL_BlitGPUTexture(command,&blit);
      auto* submitted=command; command=nullptr; acquired=false;
      auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);
      if(!fence) { SDL_WaitForGPUIdle(device); fail("submit intro presentation"); }
      const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);
      SDL_ReleaseGPUFence(device,fence);
      if(!waited) { SDL_WaitForGPUIdle(device); fail("complete intro presentation"); }
      ++presentations;
      return graphics::IntroPresentationResult::presented;
    } catch(...) { failed=true; abandon(); throw; }
  }
};
SdlIntroPresentation::SdlIntroPresentation(SDL_GPUDevice* device,SDL_Window* window,
    std::uint64_t renderer_identity,SDL_GPUTextureFormat depth_format) {
  if(!device||!window||renderer_identity==0) throw std::runtime_error("intro presentation requires device, claimed window and renderer identity");
  if(depth_format!=SDL_GPU_TEXTUREFORMAT_D16_UNORM&&depth_format!=SDL_GPU_TEXTUREFORMAT_D24_UNORM&&
     depth_format!=SDL_GPU_TEXTUREFORMAT_D32_FLOAT&&!stencil(depth_format))
    throw std::runtime_error("intro presentation requires an explicit depth attachment format");
  impl_=std::make_unique<Impl>(device,window,renderer_identity,depth_format);
  auto& p=*impl_;
  if(!SDL_GetWindowSizeInPixels(window,&p.width,&p.height)) fail("query intro presentation size");
  if(p.width<=0||p.height<=0||p.width>0x1000000||p.height>0x1000000||
     (p.color_format!=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM&&p.color_format!=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM))
    throw std::runtime_error("intro presentation requires positive exact dimensions and linear RGBA8/BGRA8 swapchain");
  p.clearer=std::make_unique<SdlPictureClear>(device);
  SDL_GPUTextureCreateInfo t{};
  t.type=SDL_GPU_TEXTURETYPE_2D; t.format=p.color_format; t.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
  t.width=static_cast<Uint32>(p.width); t.height=static_cast<Uint32>(p.height);
  t.layer_count_or_depth=t.num_levels=1; t.sample_count=SDL_GPU_SAMPLECOUNT_1;
  p.color=SDL_CreateGPUTexture(device,&t); if(!p.color) fail("create intro presentation backbuffer");
  t.format=depth_format; t.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  p.depth=SDL_CreateGPUTexture(device,&t); if(!p.depth) fail("create intro presentation depth attachment");
}
SdlIntroPresentation::~SdlIntroPresentation()=default;
std::uint64_t SdlIntroPresentation::completed_presentations() const noexcept { return impl_->presentations; }
bool SdlIntroPresentation::failed() const noexcept { return impl_->failed; }
void SdlIntroPresentation::bind_renderer_services(graphics::IntroControllerPhaseTwoServices& s) {
  auto bound=s;
  auto* p=impl_.get();
  bound.renderer_height=[p](std::uint64_t id) { p->check(id); return p->height; };
  bound.renderer_width=[p](std::uint64_t id) { p->check(id); return p->width; };
  bound.set_viewport=[p](std::uint64_t id,const graphics::PictureDeviceViewport& v) {
    p->check(id);
    if(p->command||v.x!=0||v.y!=0||v.width!=static_cast<Uint32>(p->width)||v.height!=static_cast<Uint32>(p->height)||v.minimum_depth!=0||v.maximum_depth!=1)
      throw std::runtime_error("intro presentation supports a full-window phase-two viewport only");
    p->viewport=v; p->viewport_set=true;
  };
  bound.renderer_has_stencil=[p](std::uint64_t id) { p->check(id); return stencil(p->depth_format); };
  bound.clear=[p](std::uint64_t id,const graphics::PictureViewClear& request) { p->clear(id,request); };
  bound.present=[p](std::uint64_t id) { return p->present(id); };
  s=std::move(bound);
}
} // namespace off::platform
