#include "off/platform/sdl_intro_renderer.hpp"
#include "off/platform/sdl_active_view_host.hpp"
#include "off/platform/intro_preview_diagnostic.hpp"
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
struct SyntheticScene {
  explicit SyntheticScene(const off::data::GmsIntroCameraSource& source) : camera(source) {}
  off::graphics::FreshIntroCamera camera;
};
}
int main() {
  using namespace off::graphics; using namespace off::platform;
  try {
    rejects([] { SdlIntroRenderer renderer(nullptr,{}); });
    CameraRendererList camera_renderers;
    off::data::GmsIntroCameraSource camera_source{};
    camera_source.viewport={0,0,.5F,1}; camera_source.renderer_list_selector=1;
    camera_source.near_distance=2; camera_source.far_distance=20; camera_source.angle_degrees=90;
    FreshIntroCamera camera(camera_source);
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
    quad.local_z=10; quad.horizontal_edge_span=10; quad.vertical_edge_span=20; quad.modulation_color=0xffffffff;
    quad.u_max=quad.v_min=1;
    PictureCacheTransform transform{.basis={0,0,1,0,1,0,1,0,0}};
    auto batches=expand_picture_descriptors(std::span(&quad,1),transform);
    SdlIntroDraw draw{}; draw.batches=batches; draw.catalog_image_index=7;
    // Deliberately wrong for the selected camera. Canonical projection must
    // replace this matrix with X scale1, Y scale.5, near5/far20 and clip W=Z.
    // At Z10 the camera projects this quad to NDC [-.5,.5] in both axes.
    draw.projection={.125F,0,0,0, 0,.125F,0,0, 0,0,.5F,1, 0,0,0,0};
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
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1),true); });
    auto corrupt=batches; corrupt[0].indices[0]=99; bad=draw; bad.batches=corrupt;
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,std::span(&bad,1)); });
    // The second batch has distinct geometry AND indices. Its unused first
    // quad is offscreen; drawing indices 4..7 must select the narrow second
    // quad. Reusing either draw's zero buffer offset produces different pixels.
    auto decoy=quad; decoy.local_center_x=100;
    auto narrow=quad; narrow.local_center_x=5; narrow.vertical_edge_span=10;
    const std::array second_quads{decoy,narrow};
    auto second_batches=expand_picture_descriptors(second_quads,transform);
    second_batches[0].indices.erase(second_batches[0].indices.begin(),
                                   second_batches[0].indices.begin()+6);
    auto second=draw; second.batches=second_batches;
    second.catalog_image_index=9; second.scissor={2,0,1,8};
    const std::array draws{draw,second};
    // The selected camera overrides the deliberately full-target draw viewport.
    // Its lifetime ends at renderer destruction, while the frame owns the copy.
    const CameraDisplayDimensions display{8,8,false,false};
    gpu.renderer->attach_camera_display(camera_renderers,display);
    auto* const camera_node=camera_renderers.select(1);
    require(camera_node!=nullptr,"camera renderer node registration");
    camera_node->bind(camera,static_cast<float>(camera.priority()));
    gpu.renderer->set_camera_display_pixels(16,8);
    require(!camera_renderers.head()->aspect(),"display refresh preserves construction metadata snapshot");
    gpu.renderer->set_camera_display_pixels(8,8);
    rejects([&] { gpu.renderer->set_camera_display_pixels(0,8); });
    rejects([&] { gpu.renderer->set_camera_display_pixels(UINT32_MAX,8); });
    apply_camera_display(camera,{camera_renderers,display,[]{return false;}});
    auto invalid_camera=camera_source; invalid_camera.far_distance=4;
    camera.apply_authored_parameters(invalid_camera);
    rejects([&] { (void)gpu.renderer->prepare(gpu.command,draws); });
    auto view_camera=std::make_shared<FreshIntroCamera>(camera_source);
    apply_camera_display(*view_camera,{camera_renderers,display,[]{return false;}});
    camera_node->bind(camera,static_cast<float>(camera.priority())); // still invalid
    const auto view=gpu.renderer->register_view(17,view_camera);
    std::weak_ptr<FreshIntroCamera> retained=view_camera;
    view_camera.reset();
    const auto traversal=gpu.renderer->begin_view_traversal(17);
    rejects([&] { gpu.renderer->unregister_view(view); });
    rejects([&] { gpu.renderer->retire_scene_views(17); });
    rejects([&] { (void)gpu.renderer->register_view(17,retained.lock()); });
    {
      auto other_renderer=std::make_unique<SdlIntroRenderer>(gpu.device,std::span<const IntroPreparedImage>{});
      auto other_camera=std::make_shared<FreshIntroCamera>(camera_source);
      const auto foreign_view=other_renderer->register_view(17,other_camera);
      const auto foreign_traversal=other_renderer->begin_view_traversal(17);
      rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,draws,foreign_view,traversal); });
      rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,draws,view,foreign_traversal); });
      rejects([&] { gpu.renderer->end_view_traversal(foreign_traversal); });
      other_renderer->end_view_traversal(foreign_traversal);
      other_renderer.reset();
      auto replacement=std::make_unique<SdlIntroRenderer>(gpu.device,std::span<const IntroPreparedImage>{});
      const auto replacement_view=replacement->register_view(17,other_camera);
      const auto replacement_traversal=replacement->begin_view_traversal(17);
      rejects([&] { (void)replacement->prepare_view(gpu.command,draws,foreign_view,replacement_traversal); });
      rejects([&] { (void)replacement->prepare_view(gpu.command,draws,replacement_view,foreign_traversal); });
      replacement->end_view_traversal(replacement_traversal);
      replacement->retire_scene_views(17);
      const auto after_retirement=replacement->begin_view_traversal(17);
      rejects([&] { (void)replacement->prepare_view(gpu.command,draws,replacement_view,after_retirement); });
      replacement->end_view_traversal(after_retirement);
    }
    bool wrong_thread_rejected=false;
    std::thread other([&] { try { gpu.renderer->end_view_traversal(traversal); }
      catch(const std::runtime_error&) { wrong_thread_rejected=true; } });
    other.join();
    require(wrong_thread_rejected,"view traversal is restricted to renderer owner thread");
    auto bad_view_draw=draw; bad_view_draw.state.fog_enabled=true;
    rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,std::span(&bad_view_draw,1),view,traversal); });
    gpu.renderer->end_view_traversal(traversal);
    rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,draws,view,traversal); });
    const auto recovered=gpu.renderer->begin_view_traversal(17);
    gpu.frame=gpu.renderer->prepare_view(gpu.command,draws,view,recovered);
    gpu.renderer->end_view_traversal(recovered);
    // Keep this prepared frame alive until the command is submitted.
    auto direct_view_frame=std::move(gpu.frame);
    const auto other_scene=gpu.renderer->begin_view_traversal(18);
    rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,draws,view,other_scene); });
    gpu.renderer->end_view_traversal(other_scene);
    gpu.renderer->retire_scene_views(17);
    require(retained.expired(),"registered view retains camera until retirement");
    rejects([&] { gpu.renderer->unregister_view(view); });
    {
      auto scene=std::make_shared<SyntheticScene>(camera_source);
      std::weak_ptr<SyntheticScene> scene_lifetime=scene;
      apply_camera_display(scene->camera,{camera_renderers,display,[]{return false;}});
      camera_node->bind(camera,static_cast<float>(camera.priority())); // invalid legacy camera
      std::shared_ptr<const FreshIntroCamera> scene_camera(scene,&scene->camera);
      SdlActiveViewHost host(*gpu.renderer);
      require(host.activate(scene,false),"active host retains scene owner");
      require(host.admit(false,true,11,scene_camera,2,{8,8})==SdlActiveViewHost::Coordinator::Outcome::service_unavailable,
          "unavailable renderer leaves host unchanged");
      require(host.deferred().empty(),"unavailable service does not queue a view");
      auto unrelated=std::make_shared<FreshIntroCamera>(camera_source);
      require(host.admit(true,true,11,unrelated,2,{8,8})==SdlActiveViewHost::Coordinator::Outcome::failure,
          "camera must share its scene owner's lifetime");
      require(host.admit(true,true,11,scene_camera,2,{8,8})==SdlActiveViewHost::Coordinator::Outcome::deferred,
          "unready scene defers camera");
      auto requests=host.deferred(); require(requests.size()==1,"one scene-bound request");
      const auto grant=host.issue_grant(requests.front().id); require(grant.has_value(),"scene-issued grant");
      requests.clear();
      host.set_ready(true);
      const auto outcomes=host.process_deferred(std::span(&*grant,1),{8,8},true,true);
      require(outcomes.size()==1 && outcomes.front()==SdlActiveViewHost::Coordinator::Outcome::accepted,
          "grant materializes actual renderer view");
      const auto replay=host.process_deferred(std::span(&*grant,1),{8,8},true,true);
      require(replay.size()==1 && replay.front()==SdlActiveViewHost::Coordinator::Outcome::failure,
          "scene grant cannot be replayed");
      require(host.admit(true,true,12,scene_camera,1,{8,8})==SdlActiveViewHost::Coordinator::Outcome::accepted,
          "direct renderer view accepted");
      const auto external=gpu.renderer->begin_view_traversal(999);
      require(!host.begin_traversal(),"external renderer traversal blocks host traversal");
      require(host.admit(true,true,13,scene_camera,3,{8,8})==SdlActiveViewHost::Coordinator::Outcome::failure,
          "external traversal blocks host mutation");
      gpu.renderer->end_view_traversal(external);
      const auto host_traversal=host.begin_traversal(); require(host_traversal.has_value(),"host traversal");
      require(host.admit(true,true,13,scene_camera,3,{8,8})==SdlActiveViewHost::Coordinator::Outcome::failure,
          "admission cannot mutate an active traversal");
      scene_camera.reset(); scene.reset();
      require(!scene_lifetime.expired(),"scene remains owned through traversal");
      const auto host_views=host.views(*host_traversal);
      require(host_views.size()==2 && host_views[0].camera_identity==12 && host_views[1].camera_identity==11,
          "host exposes admitted views in priority order");
      gpu.frame=host.prepare(*host_traversal,host_views[0].id,gpu.command,draws);
      host.retire();
      require(host.views(*host_traversal).empty(),"retired scene cannot expose views");
      rejects([&] { (void)host.prepare(*host_traversal,host_views[0].id,gpu.command,draws); });
      require(host.end_traversal(*host_traversal),"retired traversal ends and releases views");
      require(!host.end_traversal(*host_traversal),"ended traversal cannot be reused");
      require(scene_lifetime.expired(),"retirement releases scene after traversal");
    }
    {
      auto first_renderer=std::make_unique<SdlIntroRenderer>(gpu.device,std::span<const IntroPreparedImage>{});
      auto second_renderer=std::make_unique<SdlIntroRenderer>(gpu.device,std::span<const IntroPreparedImage>{});
      auto first_scene=std::make_shared<SyntheticScene>(camera_source);
      auto second_scene=std::make_shared<SyntheticScene>(camera_source);
      std::shared_ptr<const FreshIntroCamera> first_camera(first_scene,&first_scene->camera);
      std::shared_ptr<const FreshIntroCamera> second_camera(second_scene,&second_scene->camera);
      SdlActiveViewHost first(*first_renderer),second(*second_renderer);
      require(first.activate(first_scene,false) && second.activate(second_scene,false),"two hosts activate scenes");
      require(first.admit(true,true,1,first_camera,0,{8,8})==SdlActiveViewHost::Coordinator::Outcome::deferred &&
              second.admit(true,true,1,second_camera,0,{8,8})==SdlActiveViewHost::Coordinator::Outcome::deferred,
              "two hosts retain distinct requests");
      const auto first_requests=first.deferred(), second_requests=second.deferred();
      const auto first_grant=first.issue_grant(first_requests.front().id);
      const auto second_grant=second.issue_grant(second_requests.front().id);
      require(first_grant && second_grant,"both hosts issue grants with independent identity");
      first.set_ready(true); second.set_ready(true);
      const auto foreign=second.process_deferred(std::span(&*first_grant,1),{8,8},true,true);
      require(foreign.size()==1 && foreign.front()==SdlActiveViewHost::Coordinator::Outcome::failure,
              "foreign host grant cannot authorize a matching request");
      const auto accepted=second.process_deferred(std::span(&*second_grant,1),{8,8},true,true);
      require(accepted.size()==1 && accepted.front()==SdlActiveViewHost::Coordinator::Outcome::accepted,
              "own grant remains usable after foreign grant rejection");
    }
    {
      int deferred_reentry_rejected=0;
      auto scene=std::shared_ptr<SyntheticScene>(new SyntheticScene(camera_source),
          [&](SyntheticScene* ptr) {
            try { (void)gpu.renderer->register_view(901,std::make_shared<FreshIntroCamera>(camera_source)); }
            catch(const std::runtime_error&) { ++deferred_reentry_rejected; }
            delete ptr;
          });
      std::weak_ptr<SyntheticScene> lifetime=scene;
      std::shared_ptr<const FreshIntroCamera> camera_lease(scene,&scene->camera);
      auto host=std::make_unique<SdlActiveViewHost>(*gpu.renderer);
      require(host->activate(scene,true),"external-traversal test scene activated");
      require(host->admit(true,true,21,camera_lease,0,{8,8})==SdlActiveViewHost::Coordinator::Outcome::accepted,
              "external-traversal test view registered");
      camera_lease.reset(); scene.reset();
      const auto external=gpu.renderer->begin_view_traversal(900);
      host.reset();
      require(!lifetime.expired(),"retired view retains scene until external traversal ends");
      gpu.renderer->end_view_traversal(external);
      require(lifetime.expired(),"external traversal completion releases retired scene");
      require(deferred_reentry_rejected==1,"deferred release rejects reentrant scene deleter");
      auto later_camera=std::make_shared<FreshIntroCamera>(camera_source);
      const auto later_view=gpu.renderer->register_view(902,later_camera);
      gpu.renderer->unregister_view(later_view);
    }
    {
      auto camera_lease=std::make_shared<FreshIntroCamera>(camera_source);
      std::weak_ptr<FreshIntroCamera> lifetime=camera_lease;
      const auto retired_view=gpu.renderer->register_view(910,camera_lease);
      camera_lease.reset();
      const auto traversal_to_stop=gpu.renderer->begin_view_traversal(910);
      gpu.renderer->retire_view(retired_view);
      require(!lifetime.expired(),"retired renderer view keeps owner during traversal");
      rejects([&] { (void)gpu.renderer->prepare_view(gpu.command,draws,retired_view,traversal_to_stop); });
      gpu.renderer->end_view_traversal(traversal_to_stop);
      require(lifetime.expired(),"renderer releases retired owner after traversal");
    }
    int unregister_reentry_rejected=0;
    auto unregister_camera=std::shared_ptr<FreshIntroCamera>(new FreshIntroCamera(camera_source),
        [&](FreshIntroCamera* ptr) {
          try { gpu.renderer->retire_scene_views(31); }
          catch(const std::runtime_error&) { ++unregister_reentry_rejected; }
          delete ptr;
        });
    const auto unregister_view=gpu.renderer->register_view(31,unregister_camera);
    unregister_camera.reset();
    gpu.renderer->unregister_view(unregister_view);
    require(unregister_reentry_rejected==1,"unregister rejects deleter reentry");
    int retirement_reentry_rejected=0;
    for(int i=0;i<2;++i) {
      auto retirement_camera=std::shared_ptr<FreshIntroCamera>(new FreshIntroCamera(camera_source),
          [&](FreshIntroCamera* ptr) {
            try { (void)gpu.renderer->register_view(32,std::make_shared<FreshIntroCamera>(camera_source)); }
            catch(const std::runtime_error&) { ++retirement_reentry_rejected; }
            delete ptr;
          });
      (void)gpu.renderer->register_view(32,retirement_camera);
    }
    gpu.renderer->retire_scene_views(32);
    require(retirement_reentry_rejected==2,"scene retirement rejects each deleter reentry");
    int teardown_reentry_rejected=0;
    {
      auto renderer=std::make_unique<SdlIntroRenderer>(gpu.device,std::span<const IntroPreparedImage>{});
      auto* raw=renderer.get();
      auto teardown_camera=std::shared_ptr<FreshIntroCamera>(new FreshIntroCamera(camera_source),
          [&](FreshIntroCamera* ptr) {
            try { raw->retire_scene_views(33); }
            catch(const std::runtime_error&) { ++teardown_reentry_rejected; }
            delete ptr;
          });
      (void)renderer->register_view(33,teardown_camera);
      teardown_camera.reset();
      renderer.reset();
    }
    require(teardown_reentry_rejected==1,"renderer teardown rejects deleter reentry");
    camera.apply_authored_parameters(camera_source);
    require(gpu.frame->indexed_draw_count()==2,"two indexed draws");
    require(gpu.renderer->cached_sampler_count()==1,"identical draws share a cached sampler");
    require(gpu.renderer->cached_pipeline_count()==1,"identical draws share a cached pipeline");
    auto repeated_frame=gpu.renderer->prepare(gpu.command,draws);
    require(gpu.renderer->cached_sampler_count()==1,"same sampler state reuses cache across frames");
    require(gpu.renderer->cached_pipeline_count()==1,"same pipeline state reuses cache across frames");
    auto alternate_state=draw;
    alternate_state.state.sampler.min_filter=alternate_state.state.sampler.mag_filter=SDL_GPU_FILTER_LINEAR;
    alternate_state.state.blend.enable_color_write_mask=true;
    alternate_state.state.blend.color_write_mask=0x0f;
    auto alternate_frame=gpu.renderer->prepare(gpu.command,std::span(&alternate_state,1));
    require(gpu.renderer->cached_sampler_count()==2,"distinct sampler state gets its own cached handle");
    require(gpu.renderer->cached_pipeline_count()==2,"distinct pipeline state gets its own cached handle");
    batches.clear(); second_batches.clear(); gpu.renderer.reset(); // frame retains geometry and image owner
    require(camera_renderers.head()==nullptr,"backend destruction unlinks renderer node");
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
      const bool red=x>=1&&x<3&&y>=2&&y<6;
      const bool green=x==2&&y>=3&&y<5;
      std::array<unsigned char,4> expected{static_cast<unsigned char>(red&&!green?255:0),static_cast<unsigned char>(green?255:0),0,255};
      for(unsigned c=0;c<4;++c) if(pixels[(y*64+x)*4+c]!=expected[c]) ++mismatches;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,gpu.download); require(!mismatches,"perspective indexed image/scissor/order pixel oracle");
    // Exercise the same generic-fit diagnostic submission used by the normal
    // static fallback. The source is wholly test-authored; this is a pixel
    // witness that the fallback path can affect a cleared framebuffer.
    gpu.frame.reset();
    SDL_ReleaseGPUTexture(gpu.device,gpu.target); gpu.target=nullptr;
    SDL_ReleaseGPUTransferBuffer(gpu.device,gpu.download); gpu.download=nullptr;
    const std::array descriptors{off::data::PictureResourceDescriptor{
        .local_center_x=0.0F,.local_center_y=0.0F,.u_max=1.0F,.v_min=1.0F,
        .horizontal_edge_span=2.0F,.vertical_edge_span=2.0F,
        .modulation_color=0xffffffffU}};
    const std::array groups{off::data::PictureDrawGroup{.descriptor_span_count=1U,
                                                          .first_descriptor_index=0U}};
    const std::array bindings{off::data::PictureTextureBinding{.texture_id=1U,
                                                                 .image_index=13U}};
    off::graphics::IntroPreviewSnapshot snapshot{
        {8U,8U},{0U,off::data::PictureDrawPlan::build(descriptors,groups,bindings)},
        {{{13U,1U,{1U,1U,{255U,0U,0U,255U}}}}}};
    gpu.renderer=std::make_unique<SdlIntroRenderer>(gpu.device,snapshot.images);
    const auto submission=off::platform::IntroPreviewDiagnosticSubmission::build(
        snapshot,8U,8U,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    ti={}; ti.type=SDL_GPU_TEXTURETYPE_2D; ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET; ti.width=ti.height=8;
    ti.layer_count_or_depth=ti.num_levels=1; ti.sample_count=SDL_GPU_SAMPLECOUNT_1;
    gpu.target=SDL_CreateGPUTexture(gpu.device,&ti);
    di={SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,64*8*4,0};
    gpu.download=SDL_CreateGPUTransferBuffer(gpu.device,&di);
    require(gpu.target&&gpu.download,"create diagnostic fallback readback target");
    gpu.renderer->attach_camera_display(camera_renderers,display);
    apply_camera_display(camera,{camera_renderers,display,[]{return false;}});
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device); require(gpu.command,"acquire diagnostic command");
    // A selected camera exists but the explicit diagnostic keeps BOTH its
    // fitting matrix and full-target viewport, including geometry at Z zero.
    gpu.frame=gpu.renderer->prepare(gpu.command,submission.draws(),true);
    color={}; color.texture=gpu.target; color.load_op=SDL_GPU_LOADOP_CLEAR;
    color.store_op=SDL_GPU_STOREOP_STORE; color.clear_color={0,0,0,1};
    pass=SDL_BeginGPURenderPass(gpu.command,&color,1,nullptr); require(pass,"begin diagnostic fallback render pass");
    gpu.frame->draw(gpu.command,pass); SDL_EndGPURenderPass(pass);
    copy=SDL_BeginGPUCopyPass(gpu.command); require(copy,"begin diagnostic fallback readback");
    src={gpu.target,0,0,0,0,0,8,8,1}; dst={gpu.download,0,64,8};
    SDL_DownloadFromGPUTexture(copy,&src,&dst); SDL_EndGPUCopyPass(copy);
    submitted=gpu.command; gpu.command=nullptr;
    fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted); require(fence,"submit diagnostic fallback frame");
    waited=SDL_WaitForGPUFences(gpu.device,true,&fence,1); SDL_ReleaseGPUFence(gpu.device,fence); require(waited,"wait diagnostic fallback frame");
    pixels=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,gpu.download,false)); require(pixels,"map diagnostic fallback readback");
    unsigned diagnostic_mismatches=0;
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
      const std::array<unsigned char,4> expected{255,0,0,255};
      for(unsigned c=0;c<4;++c) if(pixels[(y*64+x)*4+c]!=expected[c]) ++diagnostic_mismatches;
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,gpu.download);
    require(!diagnostic_mismatches,"diagnostic bypass preserves fitting projection and full viewport despite camera selection");
    std::cout<<"Verified distinct vertex/index offsets, perspective projection, image ownership, ordering and scissor on 64 pixels.\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
