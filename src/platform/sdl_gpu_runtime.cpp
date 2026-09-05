#include "off/platform/sdl_gpu_runtime.hpp"

#include <SDL3/SDL.h>

#include "testgputext/shaders/shader.frag.dxil.h"
#include "testgputext/shaders/shader.frag.msl.h"
#include "testgputext/shaders/shader.frag.spv.h"
#include "testgputext/shaders/shader.vert.dxil.h"
#include "testgputext/shaders/shader.vert.msl.h"
#include "testgputext/shaders/shader.vert.spv.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace off::platform {
namespace {

struct PreviewVertex {
  std::array<float, 3> position;
  std::array<float, 4> color;
  std::array<float, 2> uv;
};

struct GpuPreview {
  SDL_GPUBuffer *vertex_buffer{nullptr};
  SDL_GPUBuffer *index_buffer{nullptr};
  SDL_GPUTexture *texture{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *pipeline{nullptr};
};

[[nodiscard]] RuntimeResult failure(const char *operation) {
  return {.success = false,
          .message = std::string(operation) + ": " + SDL_GetError()};
}

void release_preview(SDL_GPUDevice *device, GpuPreview &preview) {
  if (preview.pipeline != nullptr)
    SDL_ReleaseGPUGraphicsPipeline(device, preview.pipeline);
  if (preview.sampler != nullptr)
    SDL_ReleaseGPUSampler(device, preview.sampler);
  if (preview.texture != nullptr)
    SDL_ReleaseGPUTexture(device, preview.texture);
  if (preview.index_buffer != nullptr)
    SDL_ReleaseGPUBuffer(device, preview.index_buffer);
  if (preview.vertex_buffer != nullptr)
    SDL_ReleaseGPUBuffer(device, preview.vertex_buffer);
  preview = {};
}

struct ShaderBytes {
  const unsigned char *vertex;
  std::size_t vertex_size;
  const unsigned char *fragment;
  std::size_t fragment_size;
  const char *vertex_entrypoint;
  const char *fragment_entrypoint;
  SDL_GPUShaderFormat format;
};

[[nodiscard]] ShaderBytes shader_bytes(SDL_GPUDevice *device) {
  const auto formats = SDL_GetGPUShaderFormats(device);
  if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
    return {shader_vert_dxil,
            shader_vert_dxil_len,
            shader_frag_dxil,
            shader_frag_dxil_len,
            "VSMain",
            "PSMain",
            SDL_GPU_SHADERFORMAT_DXIL};
  }
  if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
    return {shader_vert_msl,
            shader_vert_msl_len,
            shader_frag_msl,
            shader_frag_msl_len,
            "main0",
            "main0",
            SDL_GPU_SHADERFORMAT_MSL};
  }
  return {shader_vert_spv,
          shader_vert_spv_len,
          shader_frag_spv,
          shader_frag_spv_len,
          "main",
          "main",
          SDL_GPU_SHADERFORMAT_SPIRV};
}

[[nodiscard]] SDL_GPUShader *
create_shader(SDL_GPUDevice *device, const unsigned char *bytes,
              std::size_t size, const char *entrypoint,
              SDL_GPUShaderFormat format, SDL_GPUShaderStage stage) {
  const SDL_GPUShaderCreateInfo info{
      .code_size = size,
      .code = bytes,
      .entrypoint = entrypoint,
      .format = format,
      .stage = stage,
      .num_samplers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1U : 0U,
      .num_storage_textures = 0,
      .num_storage_buffers = 0,
      .num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_VERTEX ? 1U : 0U,
      .props = 0};
  return SDL_CreateGPUShader(device, &info);
}

[[nodiscard]] std::vector<PreviewVertex>
make_preview_vertices(const graphics::RenderPreviewAsset &preview) {
  std::array<std::pair<float, std::size_t>, 3> extents{};
  for (std::size_t axis = 0; axis < extents.size(); ++axis) {
    extents[axis] = {
        preview.maximum_position[axis] - preview.minimum_position[axis], axis};
  }
  std::sort(extents.begin(), extents.end(), std::greater<>{});
  const auto horizontal = extents[0].second;
  const auto vertical = extents[1].second;
  const auto scale = 1.6F / std::max(extents[0].first, extents[1].first);
  const auto center_x = (preview.minimum_position[horizontal] +
                         preview.maximum_position[horizontal]) *
                        0.5F;
  const auto center_y = (preview.minimum_position[vertical] +
                         preview.maximum_position[vertical]) *
                        0.5F;

  std::vector<PreviewVertex> result;
  result.reserve(preview.vertices.size());
  for (const auto &source : preview.vertices) {
    result.push_back(
        {.position = {(source.position[horizontal] - center_x) * scale,
                      -(source.position[vertical] - center_y) * scale, 0.5F},
         .color = {static_cast<float>(source.color_rgba[0]) / 255.0F,
                   static_cast<float>(source.color_rgba[1]) / 255.0F,
                   static_cast<float>(source.color_rgba[2]) / 255.0F, 1.0F},
         .uv = source.texture_coordinates});
  }
  return result;
}

[[nodiscard]] bool create_pipeline(SDL_GPUDevice *device, SDL_Window *window,
                                   GpuPreview &result) {
  const auto data = shader_bytes(device);
  SDL_GPUShader *vertex = create_shader(device, data.vertex, data.vertex_size,
                                        data.vertex_entrypoint, data.format,
                                        SDL_GPU_SHADERSTAGE_VERTEX);
  SDL_GPUShader *fragment = create_shader(
      device, data.fragment, data.fragment_size, data.fragment_entrypoint,
      data.format, SDL_GPU_SHADERSTAGE_FRAGMENT);
  if (vertex == nullptr || fragment == nullptr) {
    if (fragment != nullptr)
      SDL_ReleaseGPUShader(device, fragment);
    if (vertex != nullptr)
      SDL_ReleaseGPUShader(device, vertex);
    return false;
  }
  const SDL_GPUVertexBufferDescription buffer_description{
      .slot = 0,
      .pitch = sizeof(PreviewVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      .instance_step_rate = 0};
  const std::array attributes{
      SDL_GPUVertexAttribute{.location = 0,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                             .offset = offsetof(PreviewVertex, position)},
      SDL_GPUVertexAttribute{.location = 1,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                             .offset = offsetof(PreviewVertex, color)},
      SDL_GPUVertexAttribute{.location = 2,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                             .offset = offsetof(PreviewVertex, uv)}};
  const SDL_GPUColorTargetDescription target{
      .format = SDL_GetGPUSwapchainTextureFormat(device, window),
      .blend_state = {
          .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
          .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
          .color_blend_op = SDL_GPU_BLENDOP_ADD,
          .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
          .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
          .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
          .color_write_mask = 0,
          .enable_blend = true,
          .enable_color_write_mask = false}};
  const SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertex,
      .fragment_shader = fragment,
      .vertex_input_state = {.vertex_buffer_descriptions = &buffer_description,
                             .num_vertex_buffers = 1,
                             .vertex_attributes = attributes.data(),
                             .num_vertex_attributes =
                                 static_cast<Uint32>(attributes.size())},
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
      .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                           .cull_mode = SDL_GPU_CULLMODE_NONE,
                           .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE},
      .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
      .target_info = {.color_target_descriptions = &target,
                      .num_color_targets = 1,
                      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
                      .has_depth_stencil_target = false}};
  result.pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  return result.pipeline != nullptr;
}

[[nodiscard]] bool upload_preview(SDL_GPUDevice *device, SDL_Window *window,
                                  const graphics::RenderPreviewAsset &source,
                                  GpuPreview &result) {
  const auto vertices = make_preview_vertices(source);
  const auto vertex_bytes = vertices.size() * sizeof(PreviewVertex);
  const auto index_bytes = source.indices.size() * sizeof(std::uint16_t);
  const auto texture_bytes = source.texture.pixels.size();
  const auto total_bytes = vertex_bytes + index_bytes + texture_bytes;
  if (total_bytes > std::numeric_limits<std::uint32_t>::max()) {
    SDL_SetError("render preview exceeds SDL GPU transfer limits");
    return false;
  }
  const SDL_GPUBufferCreateInfo vertex_info{
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = static_cast<Uint32>(vertex_bytes)};
  const SDL_GPUBufferCreateInfo index_info{
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
      .size = static_cast<Uint32>(index_bytes)};
  const SDL_GPUTextureCreateInfo texture_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = source.texture.width,
      .height = source.texture.height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1};
  const SDL_GPUSamplerCreateInfo sampler_info{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT};
  result.vertex_buffer = SDL_CreateGPUBuffer(device, &vertex_info);
  result.index_buffer = SDL_CreateGPUBuffer(device, &index_info);
  result.texture = SDL_CreateGPUTexture(device, &texture_info);
  result.sampler = SDL_CreateGPUSampler(device, &sampler_info);
  if (result.vertex_buffer == nullptr || result.index_buffer == nullptr ||
      result.texture == nullptr || result.sampler == nullptr ||
      !create_pipeline(device, window, result))
    return false;

  const SDL_GPUTransferBufferCreateInfo transfer_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(total_bytes)};
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr)
    return false;
  auto *mapped = static_cast<std::byte *>(
      SDL_MapGPUTransferBuffer(device, transfer, false));
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  std::memcpy(mapped, vertices.data(), vertex_bytes);
  std::memcpy(mapped + vertex_bytes, source.indices.data(), index_bytes);
  std::memcpy(mapped + vertex_bytes + index_bytes, source.texture.pixels.data(),
              texture_bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy =
      command == nullptr ? nullptr : SDL_BeginGPUCopyPass(command);
  if (copy == nullptr) {
    if (command != nullptr)
      SDL_CancelGPUCommandBuffer(command);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  const SDL_GPUTransferBufferLocation vertex_source{.transfer_buffer = transfer,
                                                    .offset = 0};
  const SDL_GPUBufferRegion vertex_target{
      .buffer = result.vertex_buffer,
      .offset = 0,
      .size = static_cast<Uint32>(vertex_bytes)};
  const SDL_GPUTransferBufferLocation index_source{
      .transfer_buffer = transfer, .offset = static_cast<Uint32>(vertex_bytes)};
  const SDL_GPUBufferRegion index_target{.buffer = result.index_buffer,
                                         .offset = 0,
                                         .size =
                                             static_cast<Uint32>(index_bytes)};
  const SDL_GPUTextureTransferInfo texture_source{
      .transfer_buffer = transfer,
      .offset = static_cast<Uint32>(vertex_bytes + index_bytes),
      .pixels_per_row = source.texture.width,
      .rows_per_layer = source.texture.height};
  const SDL_GPUTextureRegion texture_target{.texture = result.texture,
                                            .mip_level = 0,
                                            .layer = 0,
                                            .x = 0,
                                            .y = 0,
                                            .z = 0,
                                            .w = source.texture.width,
                                            .h = source.texture.height,
                                            .d = 1};
  SDL_UploadToGPUBuffer(copy, &vertex_source, &vertex_target, false);
  SDL_UploadToGPUBuffer(copy, &index_source, &index_target, false);
  SDL_UploadToGPUTexture(copy, &texture_source, &texture_target, false);
  SDL_EndGPUCopyPass(copy);
  const bool uploaded =
      SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(device);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return uploaded;
}

} // namespace

RuntimeResult run_sdl_gpu_runtime(Mode mode,
                                  const graphics::RenderPreviewAsset &preview,
                                  std::size_t frame_limit) {
  if (preview.vertices.empty() || preview.indices.empty() ||
      preview.draws.empty() || preview.texture.pixels.empty()) {
    return {.success = false, .message = "render preview is incomplete"};
  }
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    return failure("SDL initialization failed");
  SDL_Window *window =
      SDL_CreateWindow("OpenFreedomFighters", 1280, 720,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) {
    const auto result = failure("SDL window creation failed");
    SDL_Quit();
    return result;
  }
  constexpr SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL |
                                          SDL_GPU_SHADERFORMAT_MSL;
#ifndef NDEBUG
  constexpr bool debug_device = true;
#else
  constexpr bool debug_device = false;
#endif
  SDL_GPUDevice *device = SDL_CreateGPUDevice(formats, debug_device, nullptr);
  if (device == nullptr || !SDL_ClaimWindowForGPUDevice(device, window)) {
    const auto result = failure("SDL GPU device or swapchain creation failed");
    if (device != nullptr)
      SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }
  GpuPreview gpu;
  if (!upload_preview(device, window, preview, gpu)) {
    const auto result = failure("retail preview GPU upload failed");
    release_preview(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  RuntimeResult result{.success = true,
                       .message = std::string("Renderer: SDL GPU/") +
                                  SDL_GetGPUDeviceDriver(device) +
                                  " (retail preview drawn)"};
  constexpr std::array<float, 32> matrices{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool running = true;
  std::size_t frames = 0;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
        running = false;
    }
    if (!running)
      break;
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUTexture *swapchain = nullptr;
    if (command == nullptr ||
        !SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain,
                                               nullptr, nullptr)) {
      result = failure("SDL GPU frame acquisition failed");
      if (command != nullptr)
        SDL_CancelGPUCommandBuffer(command);
      break;
    }
    if (swapchain != nullptr) {
      const SDL_GPUColorTargetInfo target{
          .texture = swapchain,
          .clear_color = mode == Mode::original
                             ? SDL_FColor{0, 0, 0, 1}
                             : SDL_FColor{0.015F, 0.025F, 0.05F, 1},
          .load_op = SDL_GPU_LOADOP_CLEAR,
          .store_op = SDL_GPU_STOREOP_STORE};
      SDL_GPURenderPass *pass =
          SDL_BeginGPURenderPass(command, &target, 1, nullptr);
      if (pass == nullptr) {
        result = failure("SDL GPU render-pass creation failed");
        SDL_SubmitGPUCommandBuffer(command);
        break;
      }
      SDL_BindGPUGraphicsPipeline(pass, gpu.pipeline);
      const SDL_GPUBufferBinding vb{.buffer = gpu.vertex_buffer, .offset = 0};
      const SDL_GPUBufferBinding ib{.buffer = gpu.index_buffer, .offset = 0};
      const SDL_GPUTextureSamplerBinding tb{.texture = gpu.texture,
                                            .sampler = gpu.sampler};
      SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
      SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
      SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
      SDL_PushGPUVertexUniformData(command, 0, matrices.data(),
                                   sizeof(matrices));
      for (const auto &draw : preview.draws) {
        SDL_DrawGPUIndexedPrimitives(
            pass, static_cast<Uint32>(draw.index_count), 1,
            static_cast<Uint32>(draw.first_index), 0, 0);
      }
      SDL_EndGPURenderPass(pass);
    }
    if (!SDL_SubmitGPUCommandBuffer(command)) {
      result = failure("SDL GPU command-buffer submission failed");
      break;
    }
    ++frames;
    if (frame_limit != 0 && frames >= frame_limit)
      running = false;
  }
  SDL_WaitForGPUIdle(device);
  release_preview(device, gpu);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyGPUDevice(device);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}

} // namespace off::platform
