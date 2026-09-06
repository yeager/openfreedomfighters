#include "off/platform/sdl_picture_clear.hpp"

#include "testgputext/shaders/shader.vert.spv.h"
#include "testgputext/shaders/shader.frag.spv.h"
#include "testgputext/shaders/shader.vert.msl.h"
#include "testgputext/shaders/shader.frag.msl.h"
#include "testgputext/shaders/shader.vert.dxil.h"
#include "testgputext/shaders/shader.frag.dxil.h"

#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace off::platform {
namespace {
[[noreturn]] void failed(const char* operation) {
  throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
struct Vertex { std::array<float,3> position; std::array<float,4> color; std::array<float,2> uv; };
constexpr Uint32 vertex_offset = 256;
constexpr Uint32 vertex_bytes = sizeof(Vertex) * 6;
bool stencil_format(SDL_GPUTextureFormat format) {
  return format == SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT || format == SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
}
}

struct SdlPictureClear::Impl {
  SDL_GPUDevice* device;
  SDL_GPUShader *vertex{}, *fragment{};
  SDL_GPUTexture* white{};
  SDL_GPUSampler* sampler{};
  SDL_GPUBuffer* vertices{};
  SDL_GPUTransferBuffer* upload{};
  struct Pipeline {
    SDL_GPUTextureFormat color, depth;
    bool color_write, depth_write, stencil_write;
    SDL_GPUGraphicsPipeline* handle;
  };
  std::vector<Pipeline> pipelines;
  explicit Impl(SDL_GPUDevice* d) : device(d) {}
  ~Impl() {
    for (const auto& p : pipelines) SDL_ReleaseGPUGraphicsPipeline(device,p.handle);
    if (upload) SDL_ReleaseGPUTransferBuffer(device,upload);
    if (vertices) SDL_ReleaseGPUBuffer(device,vertices);
    if (sampler) SDL_ReleaseGPUSampler(device,sampler);
    if (white) SDL_ReleaseGPUTexture(device,white);
    if (fragment) SDL_ReleaseGPUShader(device,fragment);
    if (vertex) SDL_ReleaseGPUShader(device,vertex);
  }
  SDL_GPUGraphicsPipeline* pipeline(const SdlPictureClearTarget& target, const graphics::PictureViewClear& clear) {
    const auto depth = target.depth_stencil ? target.depth_stencil_format : SDL_GPU_TEXTUREFORMAT_INVALID;
    for (const auto& p : pipelines)
      if (p.color==target.color_format && p.depth==depth && p.color_write==clear.color &&
          p.depth_write==clear.depth && p.stencil_write==clear.stencil) return p.handle;
    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot=0; buffer.pitch=sizeof(Vertex); buffer.input_rate=SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const std::array attributes{
        SDL_GPUVertexAttribute{0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(Vertex,position)},
        SDL_GPUVertexAttribute{1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(Vertex,color)},
        SDL_GPUVertexAttribute{2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(Vertex,uv)}};
    SDL_GPUColorTargetDescription color{};
    color.format=target.color_format;
    color.blend_state.src_color_blendfactor=color.blend_state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_color_blendfactor=color.blend_state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;
    color.blend_state.color_blend_op=color.blend_state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
    color.blend_state.enable_color_write_mask=true;
    color.blend_state.color_write_mask=clear.color ?
        SDL_GPU_COLORCOMPONENT_R|SDL_GPU_COLORCOMPONENT_G|SDL_GPU_COLORCOMPONENT_B|SDL_GPU_COLORCOMPONENT_A : 0;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader=vertex; info.fragment_shader=fragment;
    info.vertex_input_state={&buffer,1,attributes.data(),static_cast<Uint32>(attributes.size())};
    info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face=SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_ALWAYS;
    info.depth_stencil_state.enable_depth_test=clear.depth;
    info.depth_stencil_state.enable_depth_write=clear.depth;
    info.depth_stencil_state.enable_stencil_test=clear.stencil;
    info.depth_stencil_state.compare_mask=0xff; info.depth_stencil_state.write_mask=clear.stencil ? 0xff : 0;
    const SDL_GPUStencilOpState stencil{SDL_GPU_STENCILOP_KEEP,SDL_GPU_STENCILOP_REPLACE,
                                      SDL_GPU_STENCILOP_KEEP,SDL_GPU_COMPAREOP_ALWAYS};
    info.depth_stencil_state.front_stencil_state=stencil; info.depth_stencil_state.back_stencil_state=stencil;
    info.target_info={&color,1,depth,target.depth_stencil!=nullptr};
    auto* result=SDL_CreateGPUGraphicsPipeline(device,&info);
    if (!result) failed("create picture clear pipeline");
    try { pipelines.push_back({target.color_format,depth,clear.color,clear.depth,clear.stencil,result}); }
    catch (...) { SDL_ReleaseGPUGraphicsPipeline(device,result); throw; }
    return result;
  }
};

SdlPictureClear::SdlPictureClear(SDL_GPUDevice* device) {
  if (!device) throw std::runtime_error("picture clear requires a live GPU device");
  impl_=std::make_unique<Impl>(device);
  auto& p=*impl_;
  const auto formats=SDL_GetGPUShaderFormats(device);
  const unsigned char *v{}, *f{}; std::size_t vs{},fs{}; const char *ve{},*fe{};
  SDL_GPUShaderFormat format{};
  if ((formats&SDL_GPU_SHADERFORMAT_DXIL)!=0) {
    v=shader_vert_dxil; vs=shader_vert_dxil_len; f=shader_frag_dxil; fs=shader_frag_dxil_len;
    ve="VSMain"; fe="PSMain"; format=SDL_GPU_SHADERFORMAT_DXIL;
  } else if ((formats&SDL_GPU_SHADERFORMAT_MSL)!=0) {
    v=shader_vert_msl; vs=shader_vert_msl_len; f=shader_frag_msl; fs=shader_frag_msl_len;
    ve=fe="main0"; format=SDL_GPU_SHADERFORMAT_MSL;
  } else if ((formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0) {
    v=shader_vert_spv; vs=shader_vert_spv_len; f=shader_frag_spv; fs=shader_frag_spv_len;
    ve=fe="main"; format=SDL_GPU_SHADERFORMAT_SPIRV;
  } else throw std::runtime_error("picture clear requires SPIR-V, MSL or DXIL shaders");
  SDL_GPUShaderCreateInfo shader{};
  shader.code=v; shader.code_size=vs; shader.entrypoint=ve; shader.format=format;
  shader.stage=SDL_GPU_SHADERSTAGE_VERTEX; shader.num_uniform_buffers=1;
  p.vertex=SDL_CreateGPUShader(device,&shader);
  if (!p.vertex) failed("create picture clear vertex shader");
  shader.code=f; shader.code_size=fs; shader.entrypoint=fe;
  shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT; shader.num_uniform_buffers=0; shader.num_samplers=1;
  p.fragment=SDL_CreateGPUShader(device,&shader);
  if (!p.fragment) failed("create picture clear fragment shader");
  SDL_GPUTextureCreateInfo texture{};
  texture.type=SDL_GPU_TEXTURETYPE_2D; texture.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER; texture.width=texture.height=texture.layer_count_or_depth=texture.num_levels=1;
  texture.sample_count=SDL_GPU_SAMPLECOUNT_1;
  p.white=SDL_CreateGPUTexture(device,&texture);
  SDL_GPUSamplerCreateInfo sampler{};
  sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
  sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  p.sampler=SDL_CreateGPUSampler(device,&sampler);
  const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_VERTEX,vertex_bytes,0};
  p.vertices=SDL_CreateGPUBuffer(device,&buffer);
  const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,vertex_offset+vertex_bytes,0};
  p.upload=SDL_CreateGPUTransferBuffer(device,&transfer);
  if (!p.white || !p.sampler || !p.vertices || !p.upload) failed("create picture clear resources");
}
SdlPictureClear::~SdlPictureClear()=default;

void SdlPictureClear::encode(SDL_GPUCommandBuffer* command, const SdlPictureClearTarget& target,
    const graphics::PictureDeviceViewport& viewport, const graphics::PictureViewClear& clear) {
  if (!command || !target.color || !viewport.width || !viewport.height ||
      (target.color_format!=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM && target.color_format!=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) ||
      viewport.x>0x1000000U || viewport.y>0x1000000U || viewport.width>0x1000000U || viewport.height>0x1000000U ||
      std::uint64_t(viewport.x)+viewport.width>target.width || std::uint64_t(viewport.y)+viewport.height>target.height ||
      target.width>static_cast<Uint32>(INT32_MAX) || target.height>static_cast<Uint32>(INT32_MAX) ||
      viewport.minimum_depth!=0 || viewport.maximum_depth!=1 || !std::isfinite(clear.depth_value) ||
      clear.depth_value<0 || clear.depth_value>1 || clear.stencil_value>255 ||
      ((clear.depth || clear.stencil) && !target.depth_stencil) ||
      (clear.stencil && !stencil_format(target.depth_stencil_format)))
    throw std::runtime_error("invalid picture clear target, viewport or attachment request");
  if (!clear.color && !clear.depth && !clear.stencil) return;
  auto& p=*impl_;
  auto* pipeline=p.pipeline(target,clear);
  const auto channel=[&](unsigned shift) { return static_cast<float>((clear.packed_color>>shift)&255U)/255.0F; };
  const std::array<float,4> color{channel(16),channel(8),channel(0),channel(24)};
  const std::array<std::array<float,2>,6> xy{{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}}};
  std::array<Vertex,6> vertices;
  for (std::size_t i=0;i<vertices.size();++i) vertices[i]={{xy[i][0],xy[i][1],clear.depth_value},color,{0,0}};
  auto* mapped=static_cast<std::byte*>(SDL_MapGPUTransferBuffer(p.device,p.upload,true));
  if (!mapped) failed("map picture clear upload");
  std::memset(mapped,255,4);
  std::memcpy(mapped+vertex_offset,vertices.data(),vertex_bytes);
  SDL_UnmapGPUTransferBuffer(p.device,p.upload);
  auto* copy=SDL_BeginGPUCopyPass(command);
  if (!copy) failed("begin picture clear upload");
  const SDL_GPUTextureTransferInfo image_source{p.upload,0,1,1};
  const SDL_GPUTextureRegion image_target{p.white,0,0,0,0,0,1,1,1};
  SDL_UploadToGPUTexture(copy,&image_source,&image_target,true);
  const SDL_GPUTransferBufferLocation buffer_source{p.upload,vertex_offset};
  const SDL_GPUBufferRegion buffer_target{p.vertices,0,vertex_bytes};
  SDL_UploadToGPUBuffer(copy,&buffer_source,&buffer_target,true);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUColorTargetInfo color_target{};
  color_target.texture=target.color; color_target.load_op=SDL_GPU_LOADOP_LOAD; color_target.store_op=SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture=target.depth_stencil; depth.load_op=SDL_GPU_LOADOP_LOAD; depth.store_op=SDL_GPU_STOREOP_STORE;
  depth.stencil_load_op=stencil_format(target.depth_stencil_format) ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op=stencil_format(target.depth_stencil_format) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
  auto* pass=SDL_BeginGPURenderPass(command,&color_target,1,target.depth_stencil ? &depth : nullptr);
  if (!pass) failed("begin picture clear draw");
  SDL_BindGPUGraphicsPipeline(pass,pipeline);
  const SDL_GPUViewport device_viewport{static_cast<float>(viewport.x),static_cast<float>(viewport.y),
      static_cast<float>(viewport.width),static_cast<float>(viewport.height),0,1};
  const SDL_Rect scissor{static_cast<int>(viewport.x),static_cast<int>(viewport.y),static_cast<int>(viewport.width),static_cast<int>(viewport.height)};
  SDL_SetGPUViewport(pass,&device_viewport); SDL_SetGPUScissor(pass,&scissor);
  SDL_SetGPUStencilReference(pass,static_cast<Uint8>(clear.stencil_value));
  const SDL_GPUBufferBinding binding{p.vertices,0}; SDL_BindGPUVertexBuffers(pass,0,&binding,1);
  const SDL_GPUTextureSamplerBinding texture{p.white,p.sampler}; SDL_BindGPUFragmentSamplers(pass,0,&texture,1);
  std::array<float,32> uniforms{};
  for (std::size_t matrix=0;matrix<2;++matrix) for(std::size_t i=0;i<4;++i) uniforms[matrix*16+i*5]=1;
  SDL_PushGPUVertexUniformData(command,0,uniforms.data(),sizeof(uniforms));
  SDL_DrawGPUPrimitives(pass,6,1,0,0);
  SDL_EndGPURenderPass(pass);
}
} // namespace off::platform
