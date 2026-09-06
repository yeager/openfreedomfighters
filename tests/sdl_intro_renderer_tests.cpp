#include "off/platform/sdl_intro_renderer.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
#include <limits>

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(std::string(message)+": "+SDL_GetError()); }
template<class F> void rejects(F f) { bool caught=false; try { f(); } catch(const std::runtime_error&) { caught=true; } require(caught,"expected input rejection"); }
struct Gpu {
  SDL_GPUDevice* device{};
  SDL_GPUTexture* target{};
  SDL_GPUTransferBuffer* download{};
  std::unique_ptr<off::platform::SdlIntroFrame> frame;
  std::unique_ptr<off::platform::SdlIntroRenderer> renderer;
  SDL_GPUCommandBuffer* command{};
  ~Gpu() {
    if(command) SDL_CancelGPUCommandBuffer(command);
    if(device) SDL_WaitForGPUIdle(device);
    frame.reset(); renderer.reset();
    if(target) SDL_ReleaseGPUTexture(device,target);
    if(download) SDL_ReleaseGPUTransferBuffer(device,download);
    if(device) SDL_DestroyGPUDevice(device);
    SDL_Quit();
  }
};
}
int main() {
  using namespace off::graphics; using namespace off::platform;
  try {
    rejects([] { SdlIntroRenderer renderer(nullptr,{}); });
    Gpu gpu;
    if(!SDL_Init(SDL_INIT_VIDEO)) { std::cout<<"SKIP: video unavailable\n"; return 77; }
    const char* driver=SDL_getenv("OFF_PICTURE_SHADER_TEST_DRIVER");
    gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL,false,driver&&*driver?driver:nullptr);
    if(!gpu.device) { std::cout<<"SKIP: GPU unavailable: "<<SDL_GetError()<<'\n'; return 77; }
    std::cout<<"Intro indexed renderer backend: "<<SDL_GetGPUDeviceDriver(gpu.device)<<'\n';
    std::array<IntroPreparedImage,2> images{{{7,123,{1,1,{255,0,0,255}}},{9,456,{1,1,{0,255,0,255}}}}};
    auto duplicate=images; duplicate[1].catalog_image_index=7;
    rejects([&] { SdlIntroRenderer bad(gpu.device,duplicate); });
    auto huge=images; huge[0].mip_zero.width=huge[0].mip_zero.height=UINT32_MAX;
    rejects([&] { SdlIntroRenderer bad(gpu.device,huge); });
    gpu.renderer=std::make_unique<SdlIntroRenderer>(gpu.device,images);
    require(gpu.renderer->image_count()==2,"catalog keyed image upload");
    // Destroy source pixels before rendering: renderer must own uploaded images.
    for(auto& image:images) image.mip_zero.pixels.clear();
    off::data::PictureQuad quad{};
    quad.local_z=2; quad.horizontal_edge_span=2; quad.vertical_edge_span=2; quad.modulation_color=0xffffffff;
    quad.u_max=quad.v_min=1;
    PictureCacheTransform transform{.basis={0,0,1,0,1,0,1,0,0}};
    auto batches=expand_picture_descriptors(std::span(&quad,1),transform);
    SdlIntroDraw draw{}; draw.batches=batches; draw.catalog_image_index=7;
    // Clip x=x, y=y, z=z/2, w=z. At z=2 the quad occupies
    // NDC [-.5,.5]^2 => exactly the central 4x4 pixels of an 8x8 target.
    draw.projection={1,0,0,0, 0,1,0,0, 0,0,.5F,1, 0,0,0,0};
    draw.viewport={0,0,8,8,0,1}; draw.scissor={0,0,8,8};
    using Op=PictureStageOperation; using Arg=PictureStageArgument;
    draw.stage={{},Op::select_argument_1,Arg::texture,Arg::diffuse,Op::select_argument_1,Arg::texture,Arg::diffuse};
    draw.state.sampler.min_filter=draw.state.sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
    draw.state.sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    draw.state.sampler.address_mode_u=draw.state.sampler.address_mode_v=draw.state.sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    draw.state.rasterizer.fill_mode=SDL_GPU_FILLMODE_FILL; draw.state.rasterizer.cull_mode=SDL_GPU_CULLMODE_NONE;
    draw.state.rasterizer.front_face=SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    draw.state.blend.src_color_blendfactor=draw.state.blend.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
    draw.state.blend.dst_color_blendfactor=draw.state.blend.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;
    draw.state.blend.color_blend_op=draw.state.blend.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
    draw.state.color_format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUTextureCreateInfo ti{}; ti.type=SDL_GPU_TEXTURETYPE_2D; ti.format=draw.state.color_format;
    ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET; ti.width=ti.height=8; ti.layer_count_or_depth=ti.num_levels=1; ti.sample_count=SDL_GPU_SAMPLECOUNT_1;
    gpu.target=SDL_CreateGPUTexture(gpu.device,&ti);
    SDL_GPUTransferBufferCreateInfo di{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,64*8*4,0};
    gpu.download=SDL_CreateGPUTransferBuffer(gpu.device,&di); require(gpu.target&&gpu.download,"create readback target");
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device); require(gpu.command,"acquire command");
    auto bad=draw; bad.state.fog_enabled=true;
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    bad=draw; bad.state.sampler.min_lod=std::numeric_limits<float>::quiet_NaN();
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    bad=draw; bad.state.blend_constants.r=std::numeric_limits<float>::infinity();
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    // SDL defines values 0..2; 3 remains within this unfixed enum's C++
    // representable range, allowing a defined unsupported-value test.
    bad=draw; bad.state.rasterizer.cull_mode=static_cast<SDL_GPUCullMode>(3);
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    bad=draw; bad.catalog_image_index=1234;
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    bad=draw; bad.projection[0]=std::numeric_limits<float>::quiet_NaN();
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    auto corrupt=batches; corrupt[0].indices[0]=99; bad=draw; bad.batches=corrupt;
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    // Two overlapping draws with different textures and scissor prove ordered
    // binding, batch-local index offsets, and per-draw state restoration.
    auto second=draw; second.catalog_image_index=9; second.scissor={4,0,4,8};
    const std::array draws{draw,second};
    gpu.frame=gpu.renderer->prepare(gpu.command,draws);
    require(gpu.frame->indexed_draw_count()==2,"two indexed draws");
    batches.clear(); gpu.renderer.reset(); // frame retains geometry and image owner
    SDL_GPUColorTargetInfo color{}; color.texture=gpu.target; color.load_op=SDL_GPU_LOADOP_CLEAR; color.store_op=SDL_GPU_STOREOP_STORE;
    color.clear_color={0,0,0,1};
    auto* pass=SDL_BeginGPURenderPass(gpu.command,&color,1,nullptr); require(pass,"begin render pass");
    gpu.frame->draw(gpu.command,pass); SDL_EndGPURenderPass(pass);
    auto* copy=SDL_BeginGPUCopyPass(gpu.command); require(copy,"begin readback");
    SDL_GPUTextureRegion src{gpu.target,0,0,0,0,0,8,8,1}; SDL_GPUTextureTransferInfo dst{gpu.download,0,64,8};
    SDL_DownloadFromGPUTexture(copy,&src,&dst); SDL_EndGPUCopyPass(copy);
    auto* submitted=gpu.command; gpu.command=nullptr;
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted); require(fence,"submit frame");
    bool waited=SDL_WaitForGPUFences(gpu.device,true,&fence,1); SDL_ReleaseGPUFence(gpu.device,fence); require(waited,"wait frame");
    auto* pixels=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,gpu.download,false)); require(pixels,"map readback");
    unsigned mismatches=0;
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
      bool inside=x>=2&&x<6&&y>=2&&y<6;
      std::array<unsigned char,4> expected{static_cast<unsigned char>(inside&&x<4?255:0),static_cast<unsigned char>(inside&&x>=4?255:0),0,255};
      for(unsigned c=0;c<4;++c) if(pixels[(y*64+x)*4+c]!=expected[c]) ++mismatches;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,gpu.download); require(!mismatches,"perspective indexed image/scissor/order pixel oracle");
    std::cout<<"Verified indexed expansion, perspective projection, image ownership, ordering and scissor on 64 pixels.\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
