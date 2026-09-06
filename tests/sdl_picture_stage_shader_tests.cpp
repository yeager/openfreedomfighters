#include "off/platform/picture_stage_shader.hpp"
#include "off/graphics/picture_draw_reset.hpp"
#include <SDL3/SDL.h>
#include "testgputext/shaders/shader.vert.spv.h"
#include "testgputext/shaders/shader.vert.msl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace off::graphics;
using Color = std::array<unsigned char, 4>;
constexpr Color texture_color{192, 64, 128, 96};
constexpr Color diffuse_color{192, 128, 64, 224};
constexpr Color factor_color{32, 224, 80, 48};
constexpr std::uint32_t factor_argb = 0x3020e050U;
constexpr Uint32 width = 8, height = 8, row_pixels = 64;
int failures = 0;
void check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}
template<class F> void rejects(F action, const char* message) {
  bool caught = false;
  try { action(); } catch (const std::exception&) { caught = true; }
  check(caught, message);
}
struct Vertex { float x, y, z, r, g, b, a, u, v; };
struct Gpu {
  SDL_GPUDevice* device{};
  SDL_GPUShader *vertex{}, *fragment{};
  SDL_GPUGraphicsPipeline* pipeline{};
  SDL_GPUTexture *target{}, *texture{};
  SDL_GPUSampler* sampler{};
  SDL_GPUBuffer* vertices{};
  SDL_GPUTransferBuffer *upload{}, *download{};
  SDL_GPUCommandBuffer* command{};
  ~Gpu() {
    if (command) SDL_CancelGPUCommandBuffer(command);
    if (device) {
      SDL_WaitForGPUIdle(device);
      if (pipeline) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
      if (vertex) SDL_ReleaseGPUShader(device, vertex);
      if (fragment) SDL_ReleaseGPUShader(device, fragment);
      if (sampler) SDL_ReleaseGPUSampler(device, sampler);
      if (target) SDL_ReleaseGPUTexture(device, target);
      if (texture) SDL_ReleaseGPUTexture(device, texture);
      if (vertices) SDL_ReleaseGPUBuffer(device, vertices);
      if (upload) SDL_ReleaseGPUTransferBuffer(device, upload);
      if (download) SDL_ReleaseGPUTransferBuffer(device, download);
      SDL_DestroyGPUDevice(device);
    }
    SDL_Quit();
  }
  void acquire() {
    command = SDL_AcquireGPUCommandBuffer(device);
    require(command != nullptr, "acquire stage test command");
  }
  void finish() {
    auto* submitted = command; command = nullptr;
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);
    require(fence != nullptr, "submit stage test");
    const bool ready = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    require(ready, "wait stage test fence");
  }
  void setup() {
    SDL_GPUShaderCreateInfo shader{};
    const auto formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
      shader.code = shader_vert_spv; shader.code_size = shader_vert_spv_len;
      shader.format = SDL_GPU_SHADERFORMAT_SPIRV; shader.entrypoint = "main";
    } else {
      shader.code = shader_vert_msl; shader.code_size = shader_vert_msl_len;
      shader.format = SDL_GPU_SHADERFORMAT_MSL; shader.entrypoint = "main0";
    }
    shader.stage = SDL_GPU_SHADERSTAGE_VERTEX; shader.num_uniform_buffers = 1;
    vertex = SDL_CreateGPUShader(device, &shader);
    fragment = off::platform::create_picture_stage_fragment_shader(device);
    require(vertex && fragment, "stage shaders");
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D; info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.width = width; info.height = height; info.layer_count_or_depth = info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1; info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    target = SDL_CreateGPUTexture(device, &info);
    info.width = info.height = 1; info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture = SDL_CreateGPUTexture(device, &info);
    SDL_GPUSamplerCreateInfo sample{};
    sample.min_filter = sample.mag_filter = SDL_GPU_FILTER_NEAREST;
    sample.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sample.address_mode_u = sample.address_mode_v = sample.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &sample);
    const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_VERTEX, sizeof(Vertex) * 6, 0};
    vertices = SDL_CreateGPUBuffer(device, &buffer);
    const SDL_GPUTransferBufferCreateInfo up{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, 256 + sizeof(Vertex) * 6, 0};
    const SDL_GPUTransferBufferCreateInfo down{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, row_pixels * height * 4, 0};
    upload = SDL_CreateGPUTransferBuffer(device, &up);
    download = SDL_CreateGPUTransferBuffer(device, &down);
    require(target && texture && sampler && vertices && upload && download, "stage resources");
    const SDL_GPUVertexBufferDescription description{0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
    const std::array attributes{
      SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, x)},
      SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, r)},
      SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, u)}};
    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color.blend_state.src_color_blendfactor = color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_color_blendfactor = color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    color.blend_state.color_blend_op = color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo pipe{};
    pipe.vertex_shader = vertex; pipe.fragment_shader = fragment;
    pipe.vertex_input_state = {&description, 1, attributes.data(), 3};
    pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipe.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipe.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipe.target_info.color_target_descriptions = &color; pipe.target_info.num_color_targets = 1;
    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe);
    require(pipeline != nullptr, "stage pipeline");
    const auto v = [](float x, float y) {
      return Vertex{x, y, 0, diffuse_color[0]/255.0F, diffuse_color[1]/255.0F,
        diffuse_color[2]/255.0F, diffuse_color[3]/255.0F, 0.5F, 0.5F};
    };
    const std::array<Vertex, 6> data{v(-1,-1), v(1,-1), v(1,1), v(-1,-1), v(1,1), v(-1,1)};
    auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device, upload, false));
    require(mapped != nullptr, "stage upload map");
    std::memcpy(mapped, texture_color.data(), 4); std::memcpy(mapped + 256, data.data(), sizeof(data));
    SDL_UnmapGPUTransferBuffer(device, upload);
    acquire();
    auto* copy = SDL_BeginGPUCopyPass(command); require(copy != nullptr, "stage upload pass");
    const SDL_GPUTextureTransferInfo source{upload, 0, 1, 1};
    const SDL_GPUTextureRegion destination{texture, 0, 0, 0, 0, 0, 1, 1, 1};
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    const SDL_GPUTransferBufferLocation vertex_source{upload, 256};
    const SDL_GPUBufferRegion vertex_target{vertices, 0, sizeof(data)};
    SDL_UploadToGPUBuffer(copy, &vertex_source, &vertex_target, false);
    SDL_EndGPUCopyPass(copy); finish();
  }
  void test(const PictureTrackedStage& stage, Color expected, const char* label,
            std::uint32_t live_factor = factor_argb) {
    const auto uniforms = off::platform::pack_picture_stage_uniforms(stage, live_factor);
    acquire();
    SDL_GPUColorTargetInfo color{};
    color.texture = target; color.clear_color = {0,0,0,0};
    color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, nullptr);
    require(pass != nullptr, "stage render pass");
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};
    const SDL_Rect scissor{0,0,int(width),int(height)};
    SDL_SetGPUViewport(pass, &viewport); SDL_SetGPUScissor(pass, &scissor);
    const SDL_GPUBufferBinding buffer{vertices, 0}; SDL_BindGPUVertexBuffers(pass, 0, &buffer, 1);
    const SDL_GPUTextureSamplerBinding sample{texture, sampler}; SDL_BindGPUFragmentSamplers(pass, 0, &sample, 1);
    std::array<float, 32> matrices{};
    for (unsigned m=0; m<2; ++m) for (unsigned i=0; i<4; ++i) matrices[m*16+i*5] = 1;
    SDL_PushGPUVertexUniformData(command, 0, matrices.data(), sizeof(matrices));
    SDL_PushGPUFragmentUniformData(command, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0); SDL_EndGPURenderPass(pass);
    auto* copy = SDL_BeginGPUCopyPass(command); require(copy != nullptr, "stage download pass");
    const SDL_GPUTextureRegion source{target, 0, 0, 0, 0, 0, width, height, 1};
    const SDL_GPUTextureTransferInfo destination{download, 0, row_pixels, height};
    SDL_DownloadFromGPUTexture(copy, &source, &destination); SDL_EndGPUCopyPass(copy); finish();
    const auto* pixels = static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device, download, false));
    require(pixels != nullptr, "stage download map");
    unsigned mismatches = 0;
    for (Uint32 y=0; y<height; ++y) for (Uint32 x=0; x<width; ++x) for (unsigned c=0; c<4; ++c)
      if (std::abs(int(pixels[(y*row_pixels+x)*4+c])-int(expected[c])) > 1) ++mismatches;
    SDL_UnmapGPUTransferBuffer(device, download);
    if (mismatches) std::cerr << label << ": " << mismatches << " mismatched channels\n";
    check(mismatches == 0, label);
  }
};
PictureTrackedStage stage(PictureStageOperation operation, PictureStageArgument first, PictureStageArgument second) {
  return {{}, operation, first, second, operation, first, second};
}
Color twice(Color a, Color b) {
  Color result{};
  for (unsigned i=0; i<4; ++i)
    result[i] = static_cast<unsigned char>(std::min(255L, std::lround(2.0*a[i]*b[i]/255.0)));
  return result;
}
Color add(Color a, Color b) {
  Color result{};
  for (unsigned i=0; i<4; ++i) result[i] = static_cast<unsigned char>(std::min(255, int(a[i])+int(b[i])));
  return result;
}
void apply(PictureTrackedStage& stage, const PictureStageRequests& requests) {
  if (requests.rgb_operation) stage.rgb_operation = *requests.rgb_operation;
  if (requests.rgb_argument_1) stage.rgb_argument_1 = *requests.rgb_argument_1;
  if (requests.rgb_argument_2) stage.rgb_argument_2 = *requests.rgb_argument_2;
  if (requests.alpha_operation) stage.alpha_operation = *requests.alpha_operation;
  if (requests.alpha_argument_1) stage.alpha_argument_1 = *requests.alpha_argument_1;
  if (requests.alpha_argument_2) stage.alpha_argument_2 = *requests.alpha_argument_2;
}
}

int main() {
  using Op = PictureStageOperation;
  using Arg = PictureStageArgument;
  try {
    const auto valid = stage(Op::select_argument_1, Arg::texture, Arg::diffuse);
    const auto packed = off::platform::pack_picture_stage_uniforms(valid, factor_argb);
    check(sizeof(packed) == 48 && alignof(decltype(packed)) == 16, "uniform layout is three aligned sixteen-byte vectors");
    for (unsigned i=0; i<4; ++i)
      check(std::abs(packed.texture_factor[i]-factor_color[i]/255.0F) < 1e-7F, "ARGB factor unpacked to RGBA");
    for (unsigned field=0; field<6; ++field) {
      auto bad = valid;
      if (field == 0) bad.rgb_operation = static_cast<Op>(255);
      if (field == 1) bad.alpha_operation = static_cast<Op>(255);
      if (field == 2) bad.rgb_argument_1 = static_cast<Arg>(255);
      if (field == 3) bad.rgb_argument_2 = static_cast<Arg>(255);
      if (field == 4) bad.alpha_argument_1 = static_cast<Arg>(255);
      if (field == 5) bad.alpha_argument_2 = static_cast<Arg>(255);
      rejects([&] { (void)off::platform::pack_picture_stage_uniforms(bad, 0); }, "invalid stage enum rejected");
    }
    auto undefined = valid; undefined.alpha_operation = Op::disable;
    rejects([&] { (void)off::platform::pack_picture_stage_uniforms(undefined, 0); }, "enabled RGB with disabled alpha rejected");
    rejects([] { (void)off::platform::create_picture_stage_fragment_shader(nullptr); }, "shader creation rejects null GPU device");
    if (failures) return 1;
    Gpu gpu;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
      std::cout << "SKIP: SDL video unavailable: " << SDL_GetError() << '\n'; return 77;
    }
    gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!gpu.device) {
      std::cout << "SKIP: SPIR-V/MSL GPU unavailable: " << SDL_GetError() << '\n'; return 77;
    }
    std::cout << "Picture stage shader backend: " << SDL_GetGPUDeviceDriver(gpu.device) << '\n';
    gpu.setup();
    gpu.test(stage(Op::select_argument_1, Arg::texture, Arg::diffuse), texture_color, "SELECTARG1 texture");
    gpu.test(stage(Op::select_argument_1, Arg::diffuse, Arg::texture), diffuse_color, "SELECTARG1 diffuse");
    gpu.test(stage(Op::select_argument_1, Arg::current, Arg::texture), diffuse_color, "stage-zero CURRENT is diffuse");
    gpu.test(stage(Op::select_argument_1, Arg::texture_factor, Arg::texture), factor_color, "SELECTARG1 texture factor");
    gpu.test(stage(Op::modulate_twice, Arg::texture, Arg::diffuse), twice(texture_color, diffuse_color), "MODULATE2X with saturated red and unsaturated alpha");
    gpu.test(stage(Op::modulate_twice, Arg::diffuse, Arg::diffuse), twice(diffuse_color, diffuse_color), "MODULATE2X alpha saturation independent from RGB");
    gpu.test(stage(Op::add, Arg::texture, Arg::diffuse), add(texture_color, diffuse_color), "ADD including alpha saturation");
    gpu.test(stage(Op::disable, Arg::texture, Arg::texture_factor), diffuse_color, "RGB DISABLE terminates stage with diffuse");
    auto mixed = stage(Op::modulate_twice, Arg::texture, Arg::current);
    mixed.alpha_operation = Op::select_argument_1; mixed.alpha_argument_1 = Arg::texture_factor;
    auto expected = twice(texture_color, diffuse_color); expected[3] = factor_color[3];
    gpu.test(mixed, expected, "RGB MODULATE2X and alpha SELECTARG1 use independent settings");
    mixed = stage(Op::select_argument_1, Arg::diffuse, Arg::texture);
    mixed.alpha_operation = Op::add; mixed.alpha_argument_1 = Arg::texture; mixed.alpha_argument_2 = Arg::texture_factor;
    expected = diffuse_color; expected[3] = texture_color[3] + factor_color[3];
    gpu.test(mixed, expected, "RGB SELECTARG1 and unsaturated alpha ADD");
    // Explicit synthetic runtime masks and inherited stage. These are not
    // authored/default material inputs or an observed original frame state.
    const PictureRendererFogState fog{0xff102030U, 0, 0, 0xff102030U};
    for (std::uint32_t features = 0; features != 4; ++features) {
      auto live = stage(Op::select_argument_1, Arg::texture_factor, Arg::current);
      const PictureMaterialStateInput input{0, 3U ^ features, 0, 0, {}, 1, true};
      const auto requests = resolve_picture_material_state(input, fog);
      check(requests.effective_features == features, "explicit masks select intended feature combination");
      apply(live, requests.resource_binding.stage_zero);
      apply(live, requests.material.stage_zero);
      const auto live_factor = requests.material.texture_factor.value_or(factor_argb);
      check(live.rgb_operation == (features == 3 ? Op::modulate_twice : Op::select_argument_1),
        "resolved material and binding requests preserve or replace inherited operation");
      check(live.rgb_argument_1 == ((features & 1U) ? Arg::texture : Arg::texture_factor),
        "features zero and two retain inherited argument selection");
      // Feature zero retains the stage but mode one explicitly changes its
      // texture factor to white; feature two retains both stage and factor.
      const Color wanted = features == 0 ? Color{255,255,255,255} :
        features == 1 ? texture_color : features == 2 ? factor_color : twice(texture_color, diffuse_color);
      const auto label = "Resolved resource/material feature " + std::to_string(features);
      gpu.test(live, wanted, label.c_str(), live_factor);
    }
    if (!failures) std::cout << "Verified 14 picture stage shader GPU cases.\n";
    return failures ? 1 : 0;
  } catch (const std::exception& error) {
    std::cerr << "Stage shader test error: " << error.what() << '\n'; return 1;
  }
}
