#pragma once

#include "off/platform/sdl_picture_clear.hpp"
#include "testgputext/shaders/shader.vert.spv.h"
#include "testgputext/shaders/shader.frag.spv.h"
#include "testgputext/shaders/shader.vert.msl.h"
#include "testgputext/shaders/shader.frag.msl.h"
#include "testgputext/shaders/shader.vert.dxil.h"
#include "testgputext/shaders/shader.frag.dxil.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace off::test {

// Independent test rasterizer: exact-depth/stencil EQUAL tests produce a white
// color mask without modifying depth/stencil. It never calls the clear helper.
class SdlGpuWitness final {
  struct Vertex { float x,y,z,r,g,b,a,u,v; };
  struct Resources {
    SDL_GPUDevice* device;
    SDL_GPUShader *vertex{}, *fragment{};
    SDL_GPUTexture* white{};
    SDL_GPUSampler* sampler{};
    SDL_GPUBuffer* vertices{};
    SDL_GPUTransferBuffer* upload{};
    std::array<SDL_GPUGraphicsPipeline*,4> pipelines{};
    explicit Resources(SDL_GPUDevice* d) : device(d) {}
    ~Resources() {
      for(auto* pipeline:pipelines) if(pipeline) SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
      if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
      if(vertices) SDL_ReleaseGPUBuffer(device,vertices);
      if(sampler) SDL_ReleaseGPUSampler(device,sampler);
      if(white) SDL_ReleaseGPUTexture(device,white);
      if(fragment) SDL_ReleaseGPUShader(device,fragment);
      if(vertex) SDL_ReleaseGPUShader(device,vertex);
    }
  };
  std::unique_ptr<Resources> p_;
  SDL_GPUTextureFormat depth_format_;
  bool uploaded_{false};
  static void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(std::string(message)+": "+SDL_GetError());
  }
public:
  SdlGpuWitness(SDL_GPUDevice* device,SDL_GPUTextureFormat depth_format)
      : p_(std::make_unique<Resources>(device)), depth_format_(depth_format) {
    require(device!=nullptr,"witness device");
    auto& p=*p_;
    const auto formats=SDL_GetGPUShaderFormats(device);
    const unsigned char *vertex{},*fragment{};
    std::size_t vertex_size{},fragment_size{};
    const char *vertex_entry{},*fragment_entry{};
    SDL_GPUShaderFormat format{};
    if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
      vertex=shader_vert_dxil; vertex_size=shader_vert_dxil_len; vertex_entry="VSMain";
      fragment=shader_frag_dxil; fragment_size=shader_frag_dxil_len; fragment_entry="PSMain"; format=SDL_GPU_SHADERFORMAT_DXIL;
    } else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
      vertex=shader_vert_msl; vertex_size=shader_vert_msl_len; vertex_entry="main0";
      fragment=shader_frag_msl; fragment_size=shader_frag_msl_len; fragment_entry="main0"; format=SDL_GPU_SHADERFORMAT_MSL;
    } else {
      require((formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0,"witness shader format");
      vertex=shader_vert_spv; vertex_size=shader_vert_spv_len; vertex_entry="main";
      fragment=shader_frag_spv; fragment_size=shader_frag_spv_len; fragment_entry="main"; format=SDL_GPU_SHADERFORMAT_SPIRV;
    }
    SDL_GPUShaderCreateInfo shader{};
    shader.code=vertex; shader.code_size=vertex_size; shader.entrypoint=vertex_entry;
    shader.format=format; shader.stage=SDL_GPU_SHADERSTAGE_VERTEX; shader.num_uniform_buffers=1;
    p.vertex=SDL_CreateGPUShader(device,&shader);
    shader.code=fragment; shader.code_size=fragment_size; shader.entrypoint=fragment_entry;
    shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT; shader.num_uniform_buffers=0; shader.num_samplers=1;
    p.fragment=SDL_CreateGPUShader(device,&shader);
    require(p.vertex && p.fragment,"witness shaders");
    SDL_GPUTextureCreateInfo texture{};
    texture.type=SDL_GPU_TEXTURETYPE_2D; texture.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width=texture.height=texture.layer_count_or_depth=texture.num_levels=1;
    texture.sample_count=SDL_GPU_SAMPLECOUNT_1;
    p.white=SDL_CreateGPUTexture(device,&texture);
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    p.sampler=SDL_CreateGPUSampler(device,&sampler);
    const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_VERTEX,sizeof(Vertex)*6,0};
    p.vertices=SDL_CreateGPUBuffer(device,&buffer);
    const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,256+sizeof(Vertex)*6,0};
    p.upload=SDL_CreateGPUTransferBuffer(device,&transfer);
    require(p.white && p.sampler && p.vertices && p.upload,"witness resources");
    const SDL_GPUVertexBufferDescription description{0,sizeof(Vertex),SDL_GPU_VERTEXINPUTRATE_VERTEX,0};
    const std::array attributes{
      SDL_GPUVertexAttribute{0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,offsetof(Vertex,x)},
      SDL_GPUVertexAttribute{1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,offsetof(Vertex,r)},
      SDL_GPUVertexAttribute{2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,offsetof(Vertex,u)}};
    SDL_GPUColorTargetDescription color{};
    color.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color.blend_state.src_color_blendfactor=color.blend_state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_color_blendfactor=color.blend_state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;
    color.blend_state.color_blend_op=color.blend_state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
    for(std::size_t i=0;i<p.pipelines.size();++i) {
      SDL_GPUGraphicsPipelineCreateInfo info{};
      info.vertex_shader=p.vertex; info.fragment_shader=p.fragment;
      info.vertex_input_state={&description,1,attributes.data(),3};
      info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL; info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
      info.rasterizer_state.front_face=SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
      info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
      info.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_EQUAL;
      info.depth_stencil_state.enable_depth_test=(i&1U)!=0;
      info.depth_stencil_state.enable_depth_write=false;
      info.depth_stencil_state.enable_stencil_test=(i&2U)!=0;
      info.depth_stencil_state.compare_mask=255; info.depth_stencil_state.write_mask=0;
      const SDL_GPUStencilOpState stencil{SDL_GPU_STENCILOP_KEEP,SDL_GPU_STENCILOP_KEEP,
                                         SDL_GPU_STENCILOP_KEEP,SDL_GPU_COMPAREOP_EQUAL};
      info.depth_stencil_state.front_stencil_state=info.depth_stencil_state.back_stencil_state=stencil;
      info.target_info={&color,1,depth_format,true};
      p.pipelines[i]=SDL_CreateGPUGraphicsPipeline(device,&info);
      require(p.pipelines[i]!=nullptr,"witness equality pipeline");
    }
  }
  void draw(SDL_GPUCommandBuffer* command,const platform::SdlPictureClearTarget& target,
            bool depth_test,float depth_value,bool stencil_test,Uint8 stencil_reference) {
    require(command && target.depth_stencil && target.depth_stencil_format==depth_format_ &&
            target.color_format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,"witness target");
    auto& p=*p_;
    if(!uploaded_) {
      // Static geometry and a white texel use their own resources, independent
      // of the helper's cycling uploads and clear pipeline.
      constexpr std::array<Vertex,6> vertices{{
        {-1,-1,0,1,1,1,1,0,0},{1,-1,0,1,1,1,1,0,0},{1,1,0,1,1,1,1,0,0},
        {-1,-1,0,1,1,1,1,0,0},{1,1,0,1,1,1,1,0,0},{-1,1,0,1,1,1,1,0,0}}};
      auto* mapped=static_cast<std::byte*>(SDL_MapGPUTransferBuffer(p.device,p.upload,false));
      require(mapped!=nullptr,"witness upload map");
      std::memset(mapped,255,4); std::memcpy(mapped+256,vertices.data(),sizeof(vertices));
      SDL_UnmapGPUTransferBuffer(p.device,p.upload);
      auto* copy=SDL_BeginGPUCopyPass(command); require(copy!=nullptr,"witness upload pass");
      const SDL_GPUTextureTransferInfo source{p.upload,0,1,1};
      const SDL_GPUTextureRegion destination{p.white,0,0,0,0,0,1,1,1};
      SDL_UploadToGPUTexture(copy,&source,&destination,false);
      const SDL_GPUTransferBufferLocation vertex_source{p.upload,256};
      const SDL_GPUBufferRegion vertex_target{p.vertices,0,sizeof(vertices)};
      SDL_UploadToGPUBuffer(copy,&vertex_source,&vertex_target,false);
      SDL_EndGPUCopyPass(copy); uploaded_=true;
    }
    SDL_GPUColorTargetInfo color{};
    color.texture=target.color; color.clear_color={0,0,0,1};
    color.load_op=SDL_GPU_LOADOP_CLEAR; color.store_op=SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture=target.depth_stencil; depth.load_op=SDL_GPU_LOADOP_LOAD; depth.store_op=SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op=SDL_GPU_LOADOP_LOAD; depth.stencil_store_op=SDL_GPU_STOREOP_STORE;
    auto* pass=SDL_BeginGPURenderPass(command,&color,1,&depth); require(pass!=nullptr,"witness render pass");
    SDL_BindGPUGraphicsPipeline(pass,p.pipelines[(depth_test?1U:0U)|(stencil_test?2U:0U)]);
    const SDL_GPUViewport viewport{0,0,static_cast<float>(target.width),static_cast<float>(target.height),0,1};
    const SDL_Rect scissor{0,0,static_cast<int>(target.width),static_cast<int>(target.height)};
    SDL_SetGPUViewport(pass,&viewport); SDL_SetGPUScissor(pass,&scissor);
    SDL_SetGPUStencilReference(pass,stencil_reference);
    const SDL_GPUBufferBinding buffer{p.vertices,0}; SDL_BindGPUVertexBuffers(pass,0,&buffer,1);
    const SDL_GPUTextureSamplerBinding sampler{p.white,p.sampler}; SDL_BindGPUFragmentSamplers(pass,0,&sampler,1);
    std::array<float,32> uniforms{};
    for(std::size_t m=0;m<2;++m) for(std::size_t i=0;i<4;++i) uniforms[m*16+i*5]=1;
    uniforms[16+14]=depth_value;
    SDL_PushGPUVertexUniformData(command,0,uniforms.data(),sizeof(uniforms));
    SDL_DrawGPUPrimitives(pass,6,1,0,0); SDL_EndGPURenderPass(pass);
  }
};
} // namespace off::test
