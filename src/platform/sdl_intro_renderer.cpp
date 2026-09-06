#include "off/platform/sdl_intro_renderer.hpp"
#include "testgputext/shaders/shader.vert.spv.h"
#include "testgputext/shaders/shader.vert.msl.h"
#include "testgputext/shaders/shader.vert.dxil.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace off::platform {
namespace {
[[noreturn]] void fail(const char* text) { throw std::runtime_error(std::string(text)+": "+SDL_GetError()); }
struct Vertex { std::array<float,3> position; std::array<float,4> color; std::array<float,2> uv; };
Uint32 bytes(std::size_t count, std::size_t stride) {
  if (count>std::numeric_limits<Uint32>::max()/stride) throw std::runtime_error("intro GPU buffer exceeds Uint32 size");
  return static_cast<Uint32>(count*stride);
}
template<class E> bool within(E value,E low,E high) { return value>=low&&value<=high; }
bool valid_state(const SdlIntroDrawState& s) {
  const auto& a=s.sampler; const auto& r=s.rasterizer; const auto& b=s.blend; const auto& d=s.depth_stencil;
  const auto compare=[](SDL_GPUCompareOp op) { return within(op,SDL_GPU_COMPAREOP_INVALID,SDL_GPU_COMPAREOP_ALWAYS); };
  const auto stencil=[&](const SDL_GPUStencilOpState& v) {
    const auto low=d.enable_stencil_test?SDL_GPU_STENCILOP_KEEP:SDL_GPU_STENCILOP_INVALID;
    return within(v.fail_op,low,SDL_GPU_STENCILOP_DECREMENT_AND_WRAP)&&within(v.pass_op,low,SDL_GPU_STENCILOP_DECREMENT_AND_WRAP)&&
      within(v.depth_fail_op,low,SDL_GPU_STENCILOP_DECREMENT_AND_WRAP)&&compare(v.compare_op)&&(!d.enable_stencil_test||v.compare_op!=SDL_GPU_COMPAREOP_INVALID);
  };
  const auto factor=[&](SDL_GPUBlendFactor v) { return within(v,b.enable_blend?SDL_GPU_BLENDFACTOR_ZERO:SDL_GPU_BLENDFACTOR_INVALID,SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE); };
  const auto blendop=[&](SDL_GPUBlendOp v) { return within(v,b.enable_blend?SDL_GPU_BLENDOP_ADD:SDL_GPU_BLENDOP_INVALID,SDL_GPU_BLENDOP_MAX); };
  const bool depth_format=s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_INVALID||s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D16_UNORM||
    s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D24_UNORM||s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D32_FLOAT||
    s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT||s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
  const bool stencil_format=s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT||s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
  return (s.color_format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM||s.color_format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)&&depth_format&&
    (!d.enable_stencil_test||stencil_format)&&within(a.min_filter,SDL_GPU_FILTER_NEAREST,SDL_GPU_FILTER_LINEAR)&&within(a.mag_filter,SDL_GPU_FILTER_NEAREST,SDL_GPU_FILTER_LINEAR)&&
    within(a.mipmap_mode,SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)&&
    within(a.address_mode_u,SDL_GPU_SAMPLERADDRESSMODE_REPEAT,SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE)&&
    within(a.address_mode_v,SDL_GPU_SAMPLERADDRESSMODE_REPEAT,SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE)&&
    within(a.address_mode_w,SDL_GPU_SAMPLERADDRESSMODE_REPEAT,SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE)&&
    !a.enable_compare&&a.props==0&&compare(a.compare_op)&&std::isfinite(a.mip_lod_bias)&&std::isfinite(a.max_anisotropy)&&
    (!a.enable_anisotropy||a.max_anisotropy>=1)&&std::isfinite(a.min_lod)&&std::isfinite(a.max_lod)&&a.min_lod>=0&&a.max_lod>=a.min_lod&&
    within(r.fill_mode,SDL_GPU_FILLMODE_FILL,SDL_GPU_FILLMODE_LINE)&&within(r.cull_mode,SDL_GPU_CULLMODE_NONE,SDL_GPU_CULLMODE_BACK)&&
    within(r.front_face,SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,SDL_GPU_FRONTFACE_CLOCKWISE)&&
    std::isfinite(r.depth_bias_constant_factor)&&std::isfinite(r.depth_bias_clamp)&&std::isfinite(r.depth_bias_slope_factor)&&
    factor(b.src_color_blendfactor)&&factor(b.dst_color_blendfactor)&&factor(b.src_alpha_blendfactor)&&factor(b.dst_alpha_blendfactor)&&
    blendop(b.color_blend_op)&&blendop(b.alpha_blend_op)&&(b.color_write_mask&0xf0)==0&&
    compare(d.compare_op)&&(!d.enable_depth_test||d.compare_op!=SDL_GPU_COMPAREOP_INVALID)&&stencil(d.front_stencil_state)&&stencil(d.back_stencil_state)&&
    std::isfinite(s.blend_constants.r)&&std::isfinite(s.blend_constants.g)&&std::isfinite(s.blend_constants.b)&&std::isfinite(s.blend_constants.a);
}
}
struct SdlIntroRenderer::Impl {
  SDL_GPUDevice* device;
  SDL_GPUShader *vertex{}, *fragment{};
  std::unordered_map<std::size_t,SDL_GPUTexture*> images;
  explicit Impl(SDL_GPUDevice* d):device(d) {}
  ~Impl() {
    for (auto [key,image]:images) { (void)key; if(image) SDL_ReleaseGPUTexture(device,image); }
    if(vertex) SDL_ReleaseGPUShader(device,vertex);
    if(fragment) SDL_ReleaseGPUShader(device,fragment);
  }
};
struct SdlIntroFrame::Impl {
  std::shared_ptr<SdlIntroRenderer::Impl> owner;
  SDL_GPUCommandBuffer* command{};
  struct Batch { Uint32 vertex_offset,index_offset,count; };
  struct Draw {
    SdlIntroDraw description;
    PictureStageShaderUniforms uniforms;
    SDL_GPUGraphicsPipeline* pipeline{};
    SDL_GPUSampler* sampler{};
    SDL_GPUTexture* texture{};
    std::vector<Batch> batches;
  };
  std::vector<Draw> draws;
  SDL_GPUBuffer *vertices{},*indices{};
  SDL_GPUTransferBuffer* upload{};
  ~Impl() {
    auto* d=owner->device;
    for(auto& draw:draws) {
      if(draw.pipeline) SDL_ReleaseGPUGraphicsPipeline(d,draw.pipeline);
      if(draw.sampler) SDL_ReleaseGPUSampler(d,draw.sampler);
    }
    if(upload) SDL_ReleaseGPUTransferBuffer(d,upload);
    if(vertices) SDL_ReleaseGPUBuffer(d,vertices);
    if(indices) SDL_ReleaseGPUBuffer(d,indices);
  }
};

SdlIntroRenderer::SdlIntroRenderer(SDL_GPUDevice* device,std::span<const graphics::IntroPreparedImage> images) {
  if(!device) throw std::runtime_error("intro renderer requires GPU device");
  impl_=std::make_shared<Impl>(device);
  std::size_t total=0;
  for(const auto& image:images) {
    const auto& p=image.mip_zero;
    const auto pixels=std::uint64_t(p.width)*p.height;
    if(!p.width||!p.height||pixels>graphics::intro_decoded_byte_budget/4||pixels*4!=p.pixels.size()||
       p.pixels.size()>graphics::intro_decoded_byte_budget-total||impl_->images.contains(image.catalog_image_index))
      throw std::runtime_error("invalid or duplicate intro GPU image");
    total+=p.pixels.size();
    impl_->images.emplace(image.catalog_image_index,nullptr);
  }
  const auto formats=SDL_GetGPUShaderFormats(device);
  SDL_GPUShaderCreateInfo shader{};
  shader.stage=SDL_GPU_SHADERSTAGE_VERTEX; shader.num_uniform_buffers=1;
  if(formats&SDL_GPU_SHADERFORMAT_DXIL) { shader.code=shader_vert_dxil; shader.code_size=shader_vert_dxil_len; shader.entrypoint="VSMain"; shader.format=SDL_GPU_SHADERFORMAT_DXIL; }
  else if(formats&SDL_GPU_SHADERFORMAT_SPIRV) { shader.code=shader_vert_spv; shader.code_size=shader_vert_spv_len; shader.entrypoint="main"; shader.format=SDL_GPU_SHADERFORMAT_SPIRV; }
  else if(formats&SDL_GPU_SHADERFORMAT_MSL) { shader.code=shader_vert_msl; shader.code_size=shader_vert_msl_len; shader.entrypoint="main0"; shader.format=SDL_GPU_SHADERFORMAT_MSL; }
  else throw std::runtime_error("intro renderer has no supported shader format");
  impl_->vertex=SDL_CreateGPUShader(device,&shader);
  if(!impl_->vertex) fail("create intro vertex shader");
  impl_->fragment=create_picture_stage_fragment_shader(device);
  // A dedicated, completed upload keeps construction independent of frame commands.
  for(const auto& image:images) {
    const auto& p=image.mip_zero;
    SDL_GPUTextureCreateInfo info{};
    info.type=SDL_GPU_TEXTURETYPE_2D; info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER; info.width=p.width; info.height=p.height;
    info.layer_count_or_depth=info.num_levels=1; info.sample_count=SDL_GPU_SAMPLECOUNT_1;
    auto*& texture=impl_->images.at(image.catalog_image_index);
    texture=SDL_CreateGPUTexture(device,&info);
    if(!texture) fail("create intro texture");
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes(p.pixels.size(),1),0};
    auto* transfer=SDL_CreateGPUTransferBuffer(device,&ti);
    if(!transfer) fail("create intro image upload");
    auto* mapped=SDL_MapGPUTransferBuffer(device,transfer,false);
    if(!mapped) { SDL_ReleaseGPUTransferBuffer(device,transfer); fail("map intro image upload"); }
    std::memcpy(mapped,p.pixels.data(),p.pixels.size()); SDL_UnmapGPUTransferBuffer(device,transfer);
    auto* command=SDL_AcquireGPUCommandBuffer(device);
    if(!command) { SDL_ReleaseGPUTransferBuffer(device,transfer); fail("acquire intro upload command"); }
    auto* copy=SDL_BeginGPUCopyPass(command);
    if(!copy) { SDL_CancelGPUCommandBuffer(command); SDL_ReleaseGPUTransferBuffer(device,transfer); fail("begin intro image copy"); }
    SDL_GPUTextureTransferInfo source{transfer,0,p.width,p.height};
    SDL_GPUTextureRegion dest{texture,0,0,0,0,0,p.width,p.height,1};
    SDL_UploadToGPUTexture(copy,&source,&dest,false); SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if(!fence) { SDL_WaitForGPUIdle(device); SDL_ReleaseGPUTransferBuffer(device,transfer); fail("submit intro image upload"); }
    const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);
    SDL_ReleaseGPUFence(device,fence); SDL_ReleaseGPUTransferBuffer(device,transfer);
    if(!waited) fail("wait for intro image upload");
  }
}
SdlIntroRenderer::~SdlIntroRenderer()=default;
std::size_t SdlIntroRenderer::image_count() const noexcept { return impl_->images.size(); }
SdlIntroFrame::SdlIntroFrame(std::unique_ptr<Impl> p):impl_(std::move(p)) {}
SdlIntroFrame::~SdlIntroFrame()=default;

std::unique_ptr<SdlIntroFrame> SdlIntroRenderer::prepare(SDL_GPUCommandBuffer* command,std::span<const SdlIntroDraw> draws) const {
  if(!command) throw std::runtime_error("intro prepare requires command buffer");
  auto p=std::make_unique<SdlIntroFrame::Impl>(); p->owner=impl_; p->command=command;
  std::vector<Vertex> vertices; std::vector<Uint16> indices;
  for(const auto& draw:draws) {
    const auto& s=draw.state; const auto& v=draw.viewport; const auto& r=draw.scissor;
    if(!valid_state(s)||s.fog_enabled||s.alpha_test_enabled||s.active_later_stages||!impl_->images.contains(draw.catalog_image_index)||
       s.color_format==SDL_GPU_TEXTUREFORMAT_INVALID||
       (s.depth_stencil_format==SDL_GPU_TEXTUREFORMAT_INVALID&&(s.depth_stencil.enable_depth_test||s.depth_stencil.enable_depth_write||s.depth_stencil.enable_stencil_test))||
       !std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.w)||!std::isfinite(v.h)||v.x<0||v.y<0||v.w<=0||v.h<=0||
       !std::isfinite(v.min_depth)||!std::isfinite(v.max_depth)||v.min_depth<0||v.max_depth>1||v.min_depth>v.max_depth||r.x<0||r.y<0||r.w<=0||r.h<=0)
      throw std::runtime_error("unsupported or invalid explicit intro draw state");
    for(float value:draw.projection) if(!std::isfinite(value)) throw std::runtime_error("nonfinite intro projection");
    SdlIntroFrame::Impl::Draw item{}; item.description=draw; item.description.batches={};
    item.uniforms=pack_picture_stage_uniforms(draw.stage,draw.packed_texture_factor);
    item.texture=impl_->images.at(draw.catalog_image_index);
    for(const auto& batch:draw.batches) {
      if(batch.vertices.empty()||batch.indices.empty()||batch.indices.size()%3||batch.vertices.size()>65536||
         batch.vertices.size()>graphics::intro_decoded_byte_budget/sizeof(Vertex)-vertices.size()||
         batch.indices.size()>graphics::intro_decoded_byte_budget/sizeof(Uint16)-indices.size()) throw std::runtime_error("invalid or oversized intro indexed batch");
      item.batches.push_back({bytes(vertices.size(),sizeof(Vertex)),bytes(indices.size(),sizeof(Uint16)),bytes(batch.indices.size(),1)});
      for(const auto& vertex:batch.vertices) {
        for(float value:vertex.position) if(!std::isfinite(value)) throw std::runtime_error("nonfinite intro vertex");
        for(float value:vertex.uv) if(!std::isfinite(value)) throw std::runtime_error("nonfinite intro UV");
        const auto c=vertex.color;
        vertices.push_back({vertex.position,{float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255},vertex.uv});
      }
      for(auto index:batch.indices) { if(index>=batch.vertices.size()) throw std::runtime_error("intro index outside batch"); indices.push_back(index); }
    }
    p->draws.push_back(std::move(item));
  }
  const Uint32 vb=bytes(vertices.size(),sizeof(Vertex)), ib=bytes(indices.size(),sizeof(Uint16));
  if(std::uint64_t(vb)+ib>std::numeric_limits<Uint32>::max()) throw std::runtime_error("intro upload exceeds Uint32 size");
  for(auto& draw:p->draws) {
    const auto& s=draw.description.state;
    draw.sampler=SDL_CreateGPUSampler(impl_->device,&s.sampler); if(!draw.sampler) fail("create intro sampler");
    SDL_GPUVertexBufferDescription buffer{0,sizeof(Vertex),SDL_GPU_VERTEXINPUTRATE_VERTEX,0};
    const std::array attrs{SDL_GPUVertexAttribute{0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(Vertex,position)},SDL_GPUVertexAttribute{1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(Vertex,color)},SDL_GPUVertexAttribute{2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(Vertex,uv)}};
    SDL_GPUColorTargetDescription color{s.color_format,s.blend};
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader=impl_->vertex; info.fragment_shader=impl_->fragment;
    info.vertex_input_state={&buffer,1,attrs.data(),static_cast<Uint32>(attrs.size())};
    info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; info.rasterizer_state=s.rasterizer;
    info.depth_stencil_state=s.depth_stencil; info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions=&color;
    info.target_info.num_color_targets=1;
    info.target_info.depth_stencil_format=s.depth_stencil_format;
    info.target_info.has_depth_stencil_target=s.depth_stencil_format!=SDL_GPU_TEXTUREFORMAT_INVALID;
    draw.pipeline=SDL_CreateGPUGraphicsPipeline(impl_->device,&info); if(!draw.pipeline) fail("create intro draw pipeline");
  }
  if(vb) {
    SDL_GPUBufferCreateInfo vi{SDL_GPU_BUFFERUSAGE_VERTEX,vb,0}, ii{SDL_GPU_BUFFERUSAGE_INDEX,ib,0};
    p->vertices=SDL_CreateGPUBuffer(impl_->device,&vi); p->indices=SDL_CreateGPUBuffer(impl_->device,&ii);
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,vb+ib,0};
    p->upload=SDL_CreateGPUTransferBuffer(impl_->device,&ti);
    if(!p->vertices||!p->indices||!p->upload) fail("create intro geometry buffers");
    auto* mapped=static_cast<std::byte*>(SDL_MapGPUTransferBuffer(impl_->device,p->upload,false));
    if(!mapped) fail("map intro geometry upload");
    std::memcpy(mapped,vertices.data(),vb); std::memcpy(mapped+vb,indices.data(),ib); SDL_UnmapGPUTransferBuffer(impl_->device,p->upload);
  }
  // Allocate the owning frame before recording anything; no throwing work follows
  // a successful BeginGPUCopyPass, so failures cannot destroy referenced buffers.
  auto frame=std::unique_ptr<SdlIntroFrame>(new SdlIntroFrame(std::move(p)));
  auto& f=*frame->impl_;
  if(vb) {
    auto* copy=SDL_BeginGPUCopyPass(command); if(!copy) fail("begin intro geometry copy");
    SDL_GPUTransferBufferLocation vs{f.upload,0}, is{f.upload,vb};
    SDL_GPUBufferRegion vt{f.vertices,0,vb}, it{f.indices,0,ib};
    SDL_UploadToGPUBuffer(copy,&vs,&vt,false); SDL_UploadToGPUBuffer(copy,&is,&it,false); SDL_EndGPUCopyPass(copy);
  }
  return frame;
}
std::size_t SdlIntroFrame::indexed_draw_count() const noexcept {
  std::size_t n=0; for(const auto& d:impl_->draws) n+=d.batches.size(); return n;
}
void SdlIntroFrame::draw(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass) const {
  if(!command||command!=impl_->command||!pass) throw std::runtime_error("intro draw requires its prepared command and active pass");
  for(const auto& d:impl_->draws) {
    const auto& desc=d.description;
    SDL_BindGPUGraphicsPipeline(pass,d.pipeline); SDL_SetGPUViewport(pass,&desc.viewport); SDL_SetGPUScissor(pass,&desc.scissor);
    SDL_SetGPUBlendConstants(pass,desc.state.blend_constants); SDL_SetGPUStencilReference(pass,desc.state.stencil_reference);
    SDL_GPUTextureSamplerBinding texture{d.texture,d.sampler}; SDL_BindGPUFragmentSamplers(pass,0,&texture,1);
    std::array<float,32> matrix{};
    std::copy(desc.projection.begin(),desc.projection.end(),matrix.begin());
    for(std::size_t i=0;i<4;++i) matrix[16+i*5]=1;
    SDL_PushGPUVertexUniformData(command,0,matrix.data(),sizeof(matrix));
    SDL_PushGPUFragmentUniformData(command,0,&d.uniforms,sizeof(d.uniforms));
    for(const auto& batch:d.batches) {
      SDL_GPUBufferBinding vertex{impl_->vertices,batch.vertex_offset}, index{impl_->indices,batch.index_offset};
      SDL_BindGPUVertexBuffers(pass,0,&vertex,1); SDL_BindGPUIndexBuffer(pass,&index,SDL_GPU_INDEXELEMENTSIZE_16BIT);
      SDL_DrawGPUIndexedPrimitives(pass,batch.count,1,0,0,0);
    }
  }
}
} // namespace off::platform
