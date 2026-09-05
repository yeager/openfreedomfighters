#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/ui/graphics_menu_draw.hpp"

#include <SDL3/SDL.h>

#include "testgputext/shaders/shader.frag.dxil.h"
#include "testgputext/shaders/shader.frag.msl.h"
#include "testgputext/shaders/shader.frag.spv.h"
#include "testgputext/shaders/shader.vert.dxil.h"
#include "testgputext/shaders/shader.vert.msl.h"
#include "testgputext/shaders/shader.vert.spv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <optional>
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

struct OverlayBatch {
  std::vector<PreviewVertex> vertices;
  std::size_t rectangle_vertices{};
};

struct GpuOverlay {
  SDL_GPUBuffer *vertex_buffer{nullptr};
  SDL_GPUTexture *atlas{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *pipeline{nullptr};
  std::size_t vertex_capacity{};
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

void release_overlay(SDL_GPUDevice *device, GpuOverlay &overlay) {
  if (overlay.pipeline != nullptr)
    SDL_ReleaseGPUGraphicsPipeline(device, overlay.pipeline);
  if (overlay.sampler != nullptr)
    SDL_ReleaseGPUSampler(device, overlay.sampler);
  if (overlay.atlas != nullptr)
    SDL_ReleaseGPUTexture(device, overlay.atlas);
  if (overlay.vertex_buffer != nullptr)
    SDL_ReleaseGPUBuffer(device, overlay.vertex_buffer);
  overlay = {};
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
  const auto &instance = *preview.object_instance;
  std::vector<std::array<float, 3>> world_positions;
  world_positions.reserve(preview.vertices.size());
  for (const auto &vertex : preview.vertices) {
    world_positions.push_back(graphics::transform_source_diagnostic_position(
        instance, vertex.position));
  }

  std::array<float, 3> projected_areas{};
  for (const auto &draw : preview.draws) {
    for (std::size_t offset = 2; offset < draw.index_count; ++offset) {
      const auto &a =
          world_positions[preview.indices[draw.first_index + offset - 2]];
      const auto &b =
          world_positions[preview.indices[draw.first_index + offset - 1]];
      const auto &c =
          world_positions[preview.indices[draw.first_index + offset]];
      const std::array ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const std::array ac{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      projected_areas[0] += std::abs(ab[1] * ac[2] - ab[2] * ac[1]);
      projected_areas[1] += std::abs(ab[2] * ac[0] - ab[0] * ac[2]);
      projected_areas[2] += std::abs(ab[0] * ac[1] - ab[1] * ac[0]);
    }
  }
  const auto dropped_axis = static_cast<std::size_t>(std::distance(
      projected_areas.begin(),
      std::max_element(projected_areas.begin(), projected_areas.end())));
  const std::array<std::array<std::size_t, 2>, 3> projection_axes{
      std::array<std::size_t, 2>{1, 2}, {0, 2}, {0, 1}};
  const auto horizontal = projection_axes[dropped_axis][0];
  const auto vertical = projection_axes[dropped_axis][1];
  std::array minimum_position{std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max()};
  std::array maximum_position{std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest()};
  for (const auto index : preview.indices) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      minimum_position[axis] =
          std::min(minimum_position[axis], world_positions[index][axis]);
      maximum_position[axis] =
          std::max(maximum_position[axis], world_positions[index][axis]);
    }
  }
  const auto horizontal_extent =
      maximum_position[horizontal] - minimum_position[horizontal];
  const auto vertical_extent =
      maximum_position[vertical] - minimum_position[vertical];
  const auto scale = 1.6F / std::max(horizontal_extent, vertical_extent);
  const auto center_x =
      (minimum_position[horizontal] + maximum_position[horizontal]) * 0.5F;
  const auto center_y =
      (minimum_position[vertical] + maximum_position[vertical]) * 0.5F;

  std::vector<PreviewVertex> result;
  result.reserve(preview.vertices.size());
  for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
    const auto &source = preview.vertices[index];
    const auto &world = world_positions[index];
    result.push_back({.position = {(world[horizontal] - center_x) * scale,
                                   -(world[vertical] - center_y) * scale, 0.5F},
                      .color = {1.0F, 1.0F, 1.0F, 1.0F},
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

[[nodiscard]] bool create_overlay_pipeline(SDL_GPUDevice *device,
                                           SDL_Window *window,
                                           GpuOverlay &result) {
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
          .enable_blend = true}};
  const SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertex,
      .fragment_shader = fragment,
      .vertex_input_state = {.vertex_buffer_descriptions = &buffer_description,
                             .num_vertex_buffers = 1,
                             .vertex_attributes = attributes.data(),
                             .num_vertex_attributes =
                                 static_cast<Uint32>(attributes.size())},
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
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

[[nodiscard]] bool create_overlay(SDL_GPUDevice *device, SDL_Window *window,
                                  GpuOverlay &result) {
  const auto atlas = ui::make_diagnostic_ascii_atlas();
  std::vector<std::uint8_t> rgba(atlas.alpha.size() * 4U, 255);
  for (std::size_t i = 0; i < atlas.alpha.size(); ++i)
    rgba[i * 4U + 3U] = atlas.alpha[i];
  result.vertex_capacity =
      (ui::maximum_ui_rects + ui::maximum_ui_text_bytes) * 6U;
  const auto vertex_bytes = result.vertex_capacity * sizeof(PreviewVertex);
  if (vertex_bytes > std::numeric_limits<Uint32>::max())
    return false;
  const SDL_GPUBufferCreateInfo buffer_info{
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = static_cast<Uint32>(vertex_bytes)};
  const SDL_GPUTextureCreateInfo texture_info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = atlas.extent.width,
      .height = atlas.extent.height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1};
  const SDL_GPUSamplerCreateInfo sampler_info{
      .min_filter = SDL_GPU_FILTER_NEAREST,
      .mag_filter = SDL_GPU_FILTER_NEAREST,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
  result.vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
  result.atlas = SDL_CreateGPUTexture(device, &texture_info);
  result.sampler = SDL_CreateGPUSampler(device, &sampler_info);
  if (result.vertex_buffer == nullptr || result.atlas == nullptr ||
      result.sampler == nullptr ||
      !create_overlay_pipeline(device, window, result))
    return false;
  const SDL_GPUTransferBufferCreateInfo transfer_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(rgba.size())};
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr)
    return false;
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  std::memcpy(mapped, rgba.data(), rgba.size());
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
  const SDL_GPUTextureTransferInfo source{.transfer_buffer = transfer,
                                          .pixels_per_row = atlas.extent.width,
                                          .rows_per_layer =
                                              atlas.extent.height};
  const SDL_GPUTextureRegion destination{.texture = result.atlas,
                                         .mip_level = 0,
                                         .layer = 0,
                                         .x = 0,
                                         .y = 0,
                                         .z = 0,
                                         .w = atlas.extent.width,
                                         .h = atlas.extent.height,
                                         .d = 1};
  SDL_UploadToGPUTexture(copy, &source, &destination, false);
  SDL_EndGPUCopyPass(copy);
  const bool success =
      SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(device);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return success;
}

void add_ui_quad(std::vector<PreviewVertex> &vertices, const ui::UiRect &rect,
                 const ui::UiColor &color, ui::UiExtent extent, float u0,
                 float v0, float u1, float v1) {
  const auto ndc_x = [&](float x) {
    return 2.0F * x / static_cast<float>(extent.width) - 1.0F;
  };
  const auto ndc_y = [&](float y) {
    return 1.0F - 2.0F * y / static_cast<float>(extent.height);
  };
  const std::array c{color.red / 255.0F, color.green / 255.0F,
                     color.blue / 255.0F, color.alpha / 255.0F};
  const float left = ndc_x(rect.x), right = ndc_x(rect.x + rect.width);
  const float top = ndc_y(rect.y), bottom = ndc_y(rect.y + rect.height);
  vertices.insert(vertices.end(), {{{left, top, 0}, c, {u0, v0}},
                                   {{right, top, 0}, c, {u1, v0}},
                                   {{right, bottom, 0}, c, {u1, v1}},
                                   {{left, top, 0}, c, {u0, v0}},
                                   {{right, bottom, 0}, c, {u1, v1}},
                                   {{left, bottom, 0}, c, {u0, v1}}});
}

[[nodiscard]] OverlayBatch
build_overlay_batch(const ui::GraphicsMenuDrawList &list) {
  OverlayBatch batch;
  batch.vertices.reserve((list.rectangles.size() + ui::maximum_ui_text_bytes) *
                         6U);
  constexpr float solid_u = 120.5F / 128.0F;
  constexpr float solid_v = 80.5F / 96.0F;
  for (const auto &command : list.rectangles)
    add_ui_quad(batch.vertices, command.bounds, command.color, list.target,
                solid_u, solid_v, solid_u, solid_v);
  batch.rectangle_vertices = batch.vertices.size();
  for (const auto &command : list.texts) {
    float x = command.x;
    for (const unsigned char byte : command.text) {
      unsigned code =
          byte >= 32 && byte <= 126 ? byte : static_cast<unsigned>('?');
      if (code != 32) {
        const unsigned cell = code - 32;
        const float u0 = static_cast<float>((cell % 16U) * 8U) / 128.0F;
        const float v0 = static_cast<float>((cell / 16U) * 16U) / 96.0F;
        add_ui_quad(batch.vertices,
                    {x, command.y, 8.0F * list.ui_scale, 16.0F * list.ui_scale},
                    command.color, list.target, u0, v0, u0 + 8.0F / 128.0F,
                    v0 + 16.0F / 96.0F);
      }
      x += 8.0F * list.ui_scale;
    }
  }
  return batch;
}

[[nodiscard]] bool upload_overlay(SDL_GPUDevice *device,
                                  SDL_GPUCommandBuffer *command,
                                  const OverlayBatch &batch, GpuOverlay &gpu,
                                  SDL_GPUTransferBuffer *&transfer) {
  if (batch.vertices.empty())
    return true;
  if (batch.vertices.size() > gpu.vertex_capacity)
    return false;
  const auto bytes = batch.vertices.size() * sizeof(PreviewVertex);
  const SDL_GPUTransferBufferCreateInfo info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(bytes)};
  transfer = SDL_CreateGPUTransferBuffer(device, &info);
  if (transfer == nullptr)
    return false;
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr)
    return false;
  std::memcpy(mapped, batch.vertices.data(), bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
  if (copy == nullptr)
    return false;
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer};
  const SDL_GPUBufferRegion destination{.buffer = gpu.vertex_buffer,
                                        .offset = 0,
                                        .size = static_cast<Uint32>(bytes)};
  SDL_UploadToGPUBuffer(copy, &source, &destination, true);
  SDL_EndGPUCopyPass(copy);
  return true;
}

[[nodiscard]] std::optional<ui::GraphicsMenuKey> menu_key(SDL_Keycode key) {
  switch (key) {
  case SDLK_F10:
    return ui::GraphicsMenuKey::f10;
  case SDLK_ESCAPE:
    return ui::GraphicsMenuKey::escape;
  case SDLK_UP:
    return ui::GraphicsMenuKey::up;
  case SDLK_DOWN:
    return ui::GraphicsMenuKey::down;
  case SDLK_LEFT:
    return ui::GraphicsMenuKey::left;
  case SDLK_RIGHT:
    return ui::GraphicsMenuKey::right;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    return ui::GraphicsMenuKey::enter;
  case SDLK_SPACE:
    return ui::GraphicsMenuKey::space;
  default:
    return std::nullopt;
  }
}

[[nodiscard]] SDL_GPUPresentMode present_mode(settings::PresentMode mode) {
  switch (mode) {
  case settings::PresentMode::mailbox:
    return SDL_GPU_PRESENTMODE_MAILBOX;
  case settings::PresentMode::immediate:
    return SDL_GPU_PRESENTMODE_IMMEDIATE;
  case settings::PresentMode::vsync:
    return SDL_GPU_PRESENTMODE_VSYNC;
  }
  return SDL_GPU_PRESENTMODE_VSYNC;
}

[[nodiscard]] bool
apply_graphics(SDL_GPUDevice *device, SDL_Window *window,
               const settings::EffectiveGraphicsSettings &value) {
  if (!SDL_WaitForGPUIdle(device))
    return false;
  if (value.window_mode == settings::WindowMode::borderless_desktop) {
    if (!SDL_SetWindowFullscreen(window, true))
      return false;
  } else {
    if (!SDL_SetWindowFullscreen(window, false) ||
        !SDL_SetWindowSize(window, static_cast<int>(value.windowed_size.width),
                           static_cast<int>(value.windowed_size.height)))
      return false;
  }
  return SDL_SetGPUSwapchainParameters(device, window,
                                       SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       present_mode(value.present_mode));
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

RuntimeResult
run_sdl_gpu_runtime(Mode mode, const graphics::RenderPreviewAsset &preview,
                    std::size_t frame_limit, bool show_graphics_menu,
                    const std::filesystem::path &screenshot_path) {
  try {
    graphics::validate_render_preview(preview);
  } catch (const std::exception &error) {
    return {.success = false,
            .message = std::string("render preview validation failed: ") +
                       error.what()};
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
  GpuOverlay overlay;
  if (!create_overlay(device, window, overlay)) {
    const auto result = failure("graphics overlay GPU upload failed");
    release_overlay(device, overlay);
    release_preview(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  settings::GraphicsCapabilities capabilities;
  capabilities.mailbox_present = SDL_WindowSupportsGPUPresentMode(
      device, window, SDL_GPU_PRESENTMODE_MAILBOX);
  capabilities.immediate_present = SDL_WindowSupportsGPUPresentMode(
      device, window, SDL_GPU_PRESENTMODE_IMMEDIATE);
  ui::GraphicsMenuSession menu{capabilities};
  settings::RequestedGraphicsSettings initial;
  initial.profile = mode;
  const auto initial_resolution =
      settings::resolve_graphics_settings(initial, capabilities);
  menu.set_confirmed(initial, *initial_resolution.effective);
  if (show_graphics_menu) {
    static_cast<void>(menu.handle_key(ui::GraphicsMenuKey::f10, true, false));
  }
  Mode active_mode = mode;

  RuntimeResult result{.success = true,
                       .message =
                           std::string("Renderer: SDL GPU/") +
                           SDL_GetGPUDeviceDriver(device) +
                           (preview.object_instance->map_instance.has_value()
                                ? " (scene-resolved retail preview drawn)"
                                : " (diagnostic retail preview drawn)")};
  constexpr std::array<float, 32> matrices{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool running = true;
  bool screenshot_captured = screenshot_path.empty();
  std::size_t frames = 0;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
        continue;
      }
      if ((event.type == SDL_EVENT_KEY_DOWN ||
           event.type == SDL_EVENT_KEY_UP) &&
          menu_key(event.key.key).has_value()) {
        const auto key = *menu_key(event.key.key);
        ui::GraphicsMenuEffect effect = ui::GraphicsMenuEffect::none;
        if (menu.phase() == ui::GraphicsMenuPhase::confirming &&
            event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (key == ui::GraphicsMenuKey::enter ||
             key == ui::GraphicsMenuKey::space)) {
          effect = menu.confirm();
        } else {
          effect = menu.handle_key(key, event.type == SDL_EVENT_KEY_DOWN,
                                   event.key.repeat);
        }
        if (effect == ui::GraphicsMenuEffect::quit_requested) {
          running = false;
        } else if (effect == ui::GraphicsMenuEffect::apply_requested) {
          if (const auto proposal = menu.request_apply()) {
            const bool applied =
                apply_graphics(device, window, proposal->effective);
            if (!applied) {
              static_cast<void>(
                  apply_graphics(device, window, menu.confirmed_effective()));
            }
            const auto acknowledged =
                menu.acknowledge_apply(applied, ui::GraphicsClock::now());
            if (applied)
              active_mode = proposal->effective.profile;
            if (acknowledged == ui::GraphicsMenuEffect::commit_requested) {
              active_mode = menu.confirmed_effective().profile;
            }
          }
        } else if (effect == ui::GraphicsMenuEffect::revert_requested) {
          const bool restored =
              apply_graphics(device, window, menu.confirmed_effective());
          static_cast<void>(menu.acknowledge_revert(restored));
          if (restored)
            active_mode = menu.confirmed_effective().profile;
        } else if (effect == ui::GraphicsMenuEffect::commit_requested) {
          active_mode = menu.confirmed_effective().profile;
        }
      }
    }
    if (!running)
      break;
    if (menu.tick(ui::GraphicsClock::now()) ==
        ui::GraphicsMenuEffect::revert_requested) {
      const bool restored =
          apply_graphics(device, window, menu.confirmed_effective());
      static_cast<void>(menu.acknowledge_revert(restored));
      if (restored)
        active_mode = menu.confirmed_effective().profile;
    }
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUTexture *swapchain = nullptr;
    Uint32 swapchain_width = 0;
    Uint32 swapchain_height = 0;
    if (command == nullptr ||
        !SDL_WaitAndAcquireGPUSwapchainTexture(
            command, window, &swapchain, &swapchain_width, &swapchain_height)) {
      result = failure("SDL GPU frame acquisition failed");
      if (command != nullptr)
        SDL_CancelGPUCommandBuffer(command);
      break;
    }
    SDL_GPUTexture *capture_texture = nullptr;
    SDL_GPUTransferBuffer *capture_transfer = nullptr;
    Uint32 capture_row_pitch = 0;
    SDL_PixelFormat capture_pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    const bool capture_this_frame =
        !screenshot_captured && swapchain != nullptr &&
        (frame_limit == 0 || frames + 1 >= frame_limit);
    if (capture_this_frame) {
      const auto format = SDL_GetGPUSwapchainTextureFormat(device, window);
      const Uint32 texel_bytes = SDL_GPUTextureFormatTexelBlockSize(format);
      capture_pixel_format = SDL_GetPixelFormatFromGPUTextureFormat(format);
      const std::uint64_t tight_row =
          static_cast<std::uint64_t>(swapchain_width) * texel_bytes;
      const std::uint64_t aligned_row =
          (tight_row + 255U) & ~std::uint64_t{255U};
      const std::uint64_t transfer_bytes = aligned_row * swapchain_height;
      if (texel_bytes != 4 || capture_pixel_format == SDL_PIXELFORMAT_UNKNOWN ||
          aligned_row > std::numeric_limits<Uint32>::max() ||
          transfer_bytes > std::numeric_limits<Uint32>::max() ||
          !SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D,
                                        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                            SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        result = {.success = false,
                  .message = "screenshot format or dimensions are unsupported"};
        SDL_SubmitGPUCommandBuffer(command);
        break;
      }
      capture_row_pitch = static_cast<Uint32>(aligned_row);
      const SDL_GPUTextureCreateInfo texture_info{
          .type = SDL_GPU_TEXTURETYPE_2D,
          .format = format,
          .usage =
              SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
          .width = swapchain_width,
          .height = swapchain_height,
          .layer_count_or_depth = 1,
          .num_levels = 1,
          .sample_count = SDL_GPU_SAMPLECOUNT_1};
      const SDL_GPUTransferBufferCreateInfo transfer_info{
          .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
          .size = static_cast<Uint32>(transfer_bytes)};
      capture_texture = SDL_CreateGPUTexture(device, &texture_info);
      capture_transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
      if (capture_texture == nullptr || capture_transfer == nullptr) {
        result = failure("screenshot resource creation failed");
        SDL_SubmitGPUCommandBuffer(command);
        if (capture_transfer != nullptr)
          SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
        if (capture_texture != nullptr)
          SDL_ReleaseGPUTexture(device, capture_texture);
        break;
      }
    }
    SDL_GPUTexture *frame_target =
        capture_texture != nullptr ? capture_texture : swapchain;
    const auto draw_list = ui::build_graphics_menu_draw_list(
        menu, {swapchain_width, swapchain_height}, ui::GraphicsClock::now());
    const auto overlay_batch = draw_list.status == ui::UiBuildStatus::ok
                                   ? build_overlay_batch(draw_list)
                                   : OverlayBatch{};
    SDL_GPUTransferBuffer *overlay_transfer = nullptr;
    if (!upload_overlay(device, command, overlay_batch, overlay,
                        overlay_transfer)) {
      result = failure("graphics overlay frame upload failed");
      SDL_SubmitGPUCommandBuffer(command);
      if (overlay_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, overlay_transfer);
      break;
    }
    if (swapchain != nullptr) {
      const SDL_GPUColorTargetInfo target{
          .texture = frame_target,
          .clear_color = active_mode == Mode::original
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

      if (!overlay_batch.vertices.empty()) {
        const SDL_GPUColorTargetInfo overlay_target{
            .texture = frame_target,
            .load_op = SDL_GPU_LOADOP_LOAD,
            .store_op = SDL_GPU_STOREOP_STORE};
        SDL_GPURenderPass *overlay_pass =
            SDL_BeginGPURenderPass(command, &overlay_target, 1, nullptr);
        if (overlay_pass == nullptr) {
          result = failure("graphics overlay render-pass creation failed");
          SDL_SubmitGPUCommandBuffer(command);
          if (overlay_transfer != nullptr)
            SDL_ReleaseGPUTransferBuffer(device, overlay_transfer);
          break;
        }
        SDL_BindGPUGraphicsPipeline(overlay_pass, overlay.pipeline);
        const SDL_GPUBufferBinding overlay_vb{.buffer = overlay.vertex_buffer,
                                              .offset = 0};
        const SDL_GPUTextureSamplerBinding overlay_tb{
            .texture = overlay.atlas, .sampler = overlay.sampler};
        SDL_BindGPUVertexBuffers(overlay_pass, 0, &overlay_vb, 1);
        SDL_BindGPUFragmentSamplers(overlay_pass, 0, &overlay_tb, 1);
        SDL_PushGPUVertexUniformData(command, 0, matrices.data(),
                                     sizeof(matrices));
        const SDL_Rect full_scissor{0, 0, static_cast<int>(swapchain_width),
                                    static_cast<int>(swapchain_height)};
        SDL_SetGPUScissor(overlay_pass, &full_scissor);
        if (overlay_batch.rectangle_vertices != 0)
          SDL_DrawGPUPrimitives(
              overlay_pass,
              static_cast<Uint32>(overlay_batch.rectangle_vertices), 1, 0, 0);
        const auto text_vertices =
            overlay_batch.vertices.size() - overlay_batch.rectangle_vertices;
        if (text_vertices != 0) {
          SDL_Rect text_scissor = full_scissor;
          if (!draw_list.texts.empty()) {
            const auto &clip = draw_list.texts.front().clip;
            text_scissor = {static_cast<int>(std::floor(clip.x)),
                            static_cast<int>(std::floor(clip.y)),
                            static_cast<int>(std::ceil(clip.width)),
                            static_cast<int>(std::ceil(clip.height))};
          }
          SDL_SetGPUScissor(overlay_pass, &text_scissor);
          SDL_DrawGPUPrimitives(
              overlay_pass, static_cast<Uint32>(text_vertices), 1,
              static_cast<Uint32>(overlay_batch.rectangle_vertices), 0);
        }
        SDL_EndGPURenderPass(overlay_pass);
      }
      if (capture_texture != nullptr) {
        const SDL_GPUBlitInfo blit{.source = {.texture = capture_texture,
                                              .w = swapchain_width,
                                              .h = swapchain_height},
                                   .destination = {.texture = swapchain,
                                                   .w = swapchain_width,
                                                   .h = swapchain_height},
                                   .load_op = SDL_GPU_LOADOP_DONT_CARE,
                                   .flip_mode = SDL_FLIP_NONE,
                                   .filter = SDL_GPU_FILTER_NEAREST,
                                   .cycle = false};
        SDL_BlitGPUTexture(command, &blit);
        SDL_GPUCopyPass *capture_copy = SDL_BeginGPUCopyPass(command);
        if (capture_copy == nullptr) {
          result = failure("screenshot copy-pass creation failed");
          SDL_SubmitGPUCommandBuffer(command);
          SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
          SDL_ReleaseGPUTexture(device, capture_texture);
          break;
        }
        const SDL_GPUTextureRegion source{.texture = capture_texture,
                                          .w = swapchain_width,
                                          .h = swapchain_height,
                                          .d = 1};
        const SDL_GPUTextureTransferInfo destination{
            .transfer_buffer = capture_transfer,
            .pixels_per_row = capture_row_pitch / 4U,
            .rows_per_layer = swapchain_height};
        SDL_DownloadFromGPUTexture(capture_copy, &source, &destination);
        SDL_EndGPUCopyPass(capture_copy);
      }
    }
    SDL_GPUFence *capture_fence = nullptr;
    const bool submitted =
        capture_texture != nullptr
            ? (capture_fence = SDL_SubmitGPUCommandBufferAndAcquireFence(
                   command)) != nullptr
            : SDL_SubmitGPUCommandBuffer(command);
    if (!submitted) {
      result = failure("SDL GPU command-buffer submission failed");
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      break;
    }
    if (capture_fence != nullptr) {
      SDL_GPUFence *fences[]{capture_fence};
      if (!SDL_WaitForGPUFences(device, true, fences, 1)) {
        result = failure("screenshot fence wait failed");
      } else {
        void *pixels =
            SDL_MapGPUTransferBuffer(device, capture_transfer, false);
        if (pixels == nullptr) {
          result = failure("screenshot transfer mapping failed");
        } else {
          SDL_Surface *surface = SDL_CreateSurfaceFrom(
              static_cast<int>(swapchain_width),
              static_cast<int>(swapchain_height), capture_pixel_format, pixels,
              static_cast<int>(capture_row_pitch));
          auto temporary_path = screenshot_path;
          temporary_path += ".part";
          const auto utf8_path = temporary_path.u8string();
          if (surface == nullptr ||
              !SDL_SaveBMP(surface,
                           reinterpret_cast<const char *>(utf8_path.c_str()))) {
            result = failure("screenshot BMP save failed");
          } else {
            std::error_code rename_error;
            std::filesystem::rename(temporary_path, screenshot_path,
                                    rename_error);
            if (rename_error) {
              std::filesystem::remove(temporary_path);
              result = {.success = false,
                        .message = "screenshot finalization failed: " +
                                   rename_error.message()};
            } else {
              screenshot_captured = true;
              result.message += " (screenshot saved)";
            }
          }
          if (surface != nullptr)
            SDL_DestroySurface(surface);
          SDL_UnmapGPUTransferBuffer(device, capture_transfer);
        }
      }
      SDL_ReleaseGPUFence(device, capture_fence);
      SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      SDL_ReleaseGPUTexture(device, capture_texture);
      if (!result.success)
        break;
    }
    if (overlay_transfer != nullptr) {
      SDL_WaitForGPUIdle(device);
      SDL_ReleaseGPUTransferBuffer(device, overlay_transfer);
    }
    ++frames;
    if (frame_limit != 0 && frames >= frame_limit)
      running = false;
  }
  SDL_WaitForGPUIdle(device);
  release_overlay(device, overlay);
  release_preview(device, gpu);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyGPUDevice(device);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return result;
}

} // namespace off::platform
