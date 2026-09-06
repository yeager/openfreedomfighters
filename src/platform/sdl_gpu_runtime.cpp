#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/platform/sdl_menu_gamepad.hpp"
#include "off/ui/graphics_menu_draw.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

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

struct GamepadSession {
  GamepadSession() = default;
  GamepadSession(const GamepadSession &) = delete;
  GamepadSession &operator=(const GamepadSession &) = delete;
  ~GamepadSession() { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); }
};

struct PreviewVertex {
  std::array<float, 3> position;
  std::array<float, 4> color;
  std::array<float, 2> uv;
};

struct GpuSceneMesh {
  SDL_GPUBuffer *vertex_buffer{nullptr};
  SDL_GPUBuffer *index_buffer{nullptr};
};

struct GpuScene {
  std::vector<GpuSceneMesh> meshes;
  std::vector<SDL_GPUTexture *> textures;
  SDL_GPUTexture *white_texture{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *triangle_opaque{nullptr};
  SDL_GPUGraphicsPipeline *triangle_blended{nullptr};
  SDL_GPUGraphicsPipeline *line_opaque{nullptr};
  SDL_GPUGraphicsPipeline *line_blended{nullptr};
  SDL_GPUTexture *depth{nullptr};
  Uint32 depth_width{0};
  Uint32 depth_height{0};
};

struct OverlayBatch {
  struct DrawRange {
    std::size_t first_vertex{};
    std::size_t vertex_count{};
    std::optional<ui::RetailUiTextureRole> texture_role;
    std::optional<ui::UiRect> clip;
  };
  std::vector<PreviewVertex> vertices;
  std::vector<std::uint8_t> atlas_rgba;
  std::vector<DrawRange> draws;
  bool valid{true};
};

struct GpuUiTexture {
  ui::RetailUiTextureRole role{};
  SDL_GPUTexture *texture{nullptr};
};

struct GpuStartupImage {
  std::size_t catalog_image_index{};
  std::uint32_t texture_id{};
  SDL_GPUTexture *texture{nullptr};
};

struct GpuStartupImages {
  std::vector<GpuStartupImage> images;
};

struct GpuOverlay {
  SDL_GPUBuffer *vertex_buffer{nullptr};
  SDL_GPUTexture *atlas{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *pipeline{nullptr};
  SDL_IOStream *font_stream{nullptr};
  TTF_Font *font{nullptr};
  std::vector<GpuUiTexture> retail_textures;
  bool ttf_initialized{false};
  std::size_t vertex_capacity{};
};

constexpr Uint32 overlay_atlas_width = 2048;
constexpr Uint32 overlay_atlas_height = 1024;

[[nodiscard]] RuntimeResult failure(const char *operation) {
  return {.success = false,
          .message = std::string(operation) + ": " + SDL_GetError()};
}

void release_scene(SDL_GPUDevice *device, GpuScene &scene) {
  if (scene.depth != nullptr)
    SDL_ReleaseGPUTexture(device, scene.depth);
  for (auto *pipeline : {scene.triangle_opaque, scene.triangle_blended,
                         scene.line_opaque, scene.line_blended}) {
    if (pipeline != nullptr)
      SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
  }
  if (scene.sampler != nullptr)
    SDL_ReleaseGPUSampler(device, scene.sampler);
  if (scene.white_texture != nullptr)
    SDL_ReleaseGPUTexture(device, scene.white_texture);
  for (auto *texture : scene.textures) {
    if (texture != nullptr)
      SDL_ReleaseGPUTexture(device, texture);
  }
  for (const auto &mesh : scene.meshes) {
    if (mesh.index_buffer != nullptr)
      SDL_ReleaseGPUBuffer(device, mesh.index_buffer);
    if (mesh.vertex_buffer != nullptr)
      SDL_ReleaseGPUBuffer(device, mesh.vertex_buffer);
  }
  scene = {};
}

void release_overlay(SDL_GPUDevice *device, GpuOverlay &overlay) {
  if (overlay.font != nullptr)
    TTF_CloseFont(overlay.font);
  if (overlay.font_stream != nullptr)
    SDL_CloseIO(overlay.font_stream);
  if (overlay.ttf_initialized)
    TTF_Quit();
  if (overlay.pipeline != nullptr)
    SDL_ReleaseGPUGraphicsPipeline(device, overlay.pipeline);
  for (const auto &texture : overlay.retail_textures)
    if (texture.texture != nullptr)
      SDL_ReleaseGPUTexture(device, texture.texture);
  if (overlay.sampler != nullptr)
    SDL_ReleaseGPUSampler(device, overlay.sampler);
  if (overlay.atlas != nullptr)
    SDL_ReleaseGPUTexture(device, overlay.atlas);
  if (overlay.vertex_buffer != nullptr)
    SDL_ReleaseGPUBuffer(device, overlay.vertex_buffer);
  overlay = {};
}

void release_startup_images(SDL_GPUDevice *device,
                            GpuStartupImages &startup) {
  for (const auto &image : startup.images)
    if (image.texture != nullptr)
      SDL_ReleaseGPUTexture(device, image.texture);
  startup = {};
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
                                  const ui::RetailUiFontSet &fonts,
                                  GpuOverlay &result) {
  if (fonts.fonts.empty() || !TTF_Init())
    return false;
  result.ttf_initialized = true;
  const auto &bytes = fonts.fonts.front().sfnt;
  result.font_stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
  result.font = result.font_stream == nullptr
                    ? nullptr
                    : TTF_OpenFontIO(result.font_stream, false, 24.0F);
  if (result.font == nullptr)
    return false;
  result.vertex_capacity =
      (ui::maximum_ui_rects + ui::maximum_ui_texture_commands +
       ui::maximum_ui_text_bytes) *
      6U;
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
      .width = overlay_atlas_width,
      .height = overlay_atlas_height,
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
  return true;
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

void add_clipped_ui_quad(std::vector<PreviewVertex> &vertices,
                         const ui::UiRect &rect, const ui::UiRect &clip,
                         const ui::UiColor &color, ui::UiExtent extent,
                         float u0, float v0, float u1, float v1) {
  const float left = std::max(rect.x, clip.x);
  const float top = std::max(rect.y, clip.y);
  const float right = std::min(rect.x + rect.width, clip.x + clip.width);
  const float bottom = std::min(rect.y + rect.height, clip.y + clip.height);
  if (left >= right || top >= bottom)
    return;
  const float clipped_u0 = u0 + (u1 - u0) * (left - rect.x) / rect.width;
  const float clipped_v0 = v0 + (v1 - v0) * (top - rect.y) / rect.height;
  const float clipped_u1 = u0 + (u1 - u0) * (right - rect.x) / rect.width;
  const float clipped_v1 = v0 + (v1 - v0) * (bottom - rect.y) / rect.height;
  add_ui_quad(vertices, {left, top, right - left, bottom - top}, color, extent,
              clipped_u0, clipped_v0, clipped_u1, clipped_v1);
}

[[nodiscard]] OverlayBatch
build_overlay_batch(const ui::GraphicsMenuDrawList &list, TTF_Font *font) {
  OverlayBatch batch;
  if (!TTF_SetFontSize(font, 24.0F * list.ui_scale)) {
    batch.valid = false;
    return batch;
  }
  batch.atlas_rgba.assign(static_cast<std::size_t>(overlay_atlas_width) *
                              overlay_atlas_height * 4U,
                          255U);
  for (std::size_t pixel = 0; pixel < batch.atlas_rgba.size() / 4U; ++pixel)
    batch.atlas_rgba[pixel * 4U + 3U] = 0;
  batch.atlas_rgba[3] = 255U;
  batch.vertices.reserve((list.rectangles.size() + list.textures.size() +
                          ui::maximum_ui_text_bytes) *
                         6U);
  constexpr std::size_t layer_count = 5;
  struct ClippedVertices {
    std::vector<PreviewVertex> vertices;
    ui::UiRect clip;
  };
  std::array<std::vector<PreviewVertex>, layer_count> rectangle_vertices;
  std::array<std::vector<ClippedVertices>, layer_count> text_pieces;
  constexpr float solid_u = 0.25F / static_cast<float>(overlay_atlas_width);
  constexpr float solid_v = 0.25F / static_cast<float>(overlay_atlas_height);
  for (const auto &command : list.rectangles)
    add_ui_quad(rectangle_vertices[static_cast<std::size_t>(command.layer)],
                command.bounds, command.color, list.target, solid_u, solid_v,
                solid_u, solid_v);
  Uint32 cursor_x = 1, cursor_y = 1, shelf_height = 0;
  for (const auto &command : list.texts) {
    const auto layer = static_cast<std::size_t>(command.layer);
    SDL_Surface *rendered = TTF_RenderText_Blended(
        font, command.text.data(), command.text.size(), {255, 255, 255, 255});
    SDL_Surface *surface =
        rendered == nullptr
            ? nullptr
            : SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
    if (rendered != nullptr)
      SDL_DestroySurface(rendered);
    if (surface == nullptr) {
      batch.valid = false;
      continue;
    }
    if (surface->w <= 0 || surface->h <= 0) {
      SDL_DestroySurface(surface);
      continue;
    }
    if (surface->w >= static_cast<int>(overlay_atlas_width) ||
        surface->h >= static_cast<int>(overlay_atlas_height)) {
      batch.valid = false;
      SDL_DestroySurface(surface);
      continue;
    }
    const Uint32 width = static_cast<Uint32>(surface->w);
    const Uint32 height = static_cast<Uint32>(surface->h);
    if (cursor_x + width + 1U > overlay_atlas_width) {
      cursor_x = 1;
      cursor_y += shelf_height + 1U;
      shelf_height = 0;
    }
    if (cursor_y + height + 1U <= overlay_atlas_height &&
        width + 2U <= overlay_atlas_width) {
      const auto *source = static_cast<const std::uint8_t *>(surface->pixels);
      for (Uint32 y = 0; y < height; ++y) {
        auto *destination =
            batch.atlas_rgba.data() +
            (static_cast<std::size_t>(cursor_y + y) * overlay_atlas_width +
             cursor_x) *
                4U;
        std::memcpy(destination,
                    source + static_cast<std::size_t>(y) *
                                 static_cast<std::size_t>(surface->pitch),
                    static_cast<std::size_t>(width) * 4U);
      }
      const float u0 = static_cast<float>(cursor_x) / overlay_atlas_width;
      const float v0 = static_cast<float>(cursor_y) / overlay_atlas_height;
      const float u1 =
          static_cast<float>(cursor_x + width) / overlay_atlas_width;
      const float v1 =
          static_cast<float>(cursor_y + height) / overlay_atlas_height;
      ClippedVertices piece{.clip = command.clip};
      add_clipped_ui_quad(piece.vertices,
                          {command.x, command.y, static_cast<float>(width),
                           static_cast<float>(height)},
                          command.clip, command.color, list.target, u0, v0, u1,
                          v1);
      if (!piece.vertices.empty())
        text_pieces[layer].push_back(std::move(piece));
      cursor_x += width + 1U;
      shelf_height = std::max(shelf_height, height);
    } else {
      batch.valid = false;
    }
    SDL_DestroySurface(surface);
  }
  const auto append = [&](const std::vector<PreviewVertex> &vertices,
                          std::optional<ui::RetailUiTextureRole> role,
                          std::optional<ui::UiRect> clip = std::nullopt) {
    if (vertices.empty())
      return;
    const auto first = batch.vertices.size();
    batch.vertices.insert(batch.vertices.end(), vertices.begin(),
                          vertices.end());
    if (!batch.draws.empty() && batch.draws.back().texture_role == role &&
        batch.draws.back().clip == clip &&
        batch.draws.back().first_vertex + batch.draws.back().vertex_count ==
            first) {
      batch.draws.back().vertex_count += vertices.size();
    } else {
      batch.draws.push_back({first, vertices.size(), role, clip});
    }
  };
  for (std::size_t layer = 0; layer < layer_count; ++layer) {
    append(rectangle_vertices[layer], std::nullopt);
    for (const auto &command : list.textures) {
      if (static_cast<std::size_t>(command.layer) != layer)
        continue;
      std::vector<PreviewVertex> vertices;
      vertices.reserve(6);
      add_ui_quad(vertices, command.bounds, command.color, list.target,
                  command.source.x, command.source.y,
                  command.source.x + command.source.width,
                  command.source.y + command.source.height);
      append(vertices, command.texture_role);
    }
    for (const auto &piece : text_pieces[layer])
      append(piece.vertices, std::nullopt, piece.clip);
  }
  return batch;
}

[[nodiscard]] bool upload_overlay(SDL_GPUDevice *device,
                                  SDL_GPUCommandBuffer *command,
                                  const OverlayBatch &batch, GpuOverlay &gpu,
                                  SDL_GPUTransferBuffer *&transfer,
                                  SDL_GPUTransferBuffer *&atlas_transfer) {
  if (!batch.valid)
    return false;
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
  const SDL_GPUTransferBufferCreateInfo atlas_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(batch.atlas_rgba.size())};
  atlas_transfer = SDL_CreateGPUTransferBuffer(device, &atlas_info);
  if (atlas_transfer == nullptr)
    return false;
  mapped = SDL_MapGPUTransferBuffer(device, atlas_transfer, false);
  if (mapped == nullptr)
    return false;
  std::memcpy(mapped, batch.atlas_rgba.data(), batch.atlas_rgba.size());
  SDL_UnmapGPUTransferBuffer(device, atlas_transfer);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
  if (copy == nullptr)
    return false;
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer};
  const SDL_GPUBufferRegion destination{.buffer = gpu.vertex_buffer,
                                        .offset = 0,
                                        .size = static_cast<Uint32>(bytes)};
  SDL_UploadToGPUBuffer(copy, &source, &destination, true);
  const SDL_GPUTextureTransferInfo texture_source{
      .transfer_buffer = atlas_transfer,
      .pixels_per_row = overlay_atlas_width,
      .rows_per_layer = overlay_atlas_height};
  const SDL_GPUTextureRegion texture_destination{.texture = gpu.atlas,
                                                 .w = overlay_atlas_width,
                                                 .h = overlay_atlas_height,
                                                 .d = 1};
  SDL_UploadToGPUTexture(copy, &texture_source, &texture_destination, true);
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

[[nodiscard]] SDL_GPUGraphicsPipeline *
create_scene_pipeline(SDL_GPUDevice *device, SDL_Window *window,
                      graphics::PrimitiveTopology topology, bool blended) {
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
    return nullptr;
  }
  const SDL_GPUVertexBufferDescription buffer_description{
      .slot = 0,
      .pitch = sizeof(graphics::SceneGpuVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX};
  const std::array attributes{
      SDL_GPUVertexAttribute{.location = 0,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                             .offset =
                                 offsetof(graphics::SceneGpuVertex, position)},
      SDL_GPUVertexAttribute{.location = 1,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                             .offset =
                                 offsetof(graphics::SceneGpuVertex, color)},
      SDL_GPUVertexAttribute{
          .location = 2,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(graphics::SceneGpuVertex, texture_coordinates)}};
  const SDL_GPUColorTargetDescription target{
      .format = SDL_GetGPUSwapchainTextureFormat(device, window),
      .blend_state = {
          .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
          .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
          .color_blend_op = SDL_GPU_BLENDOP_ADD,
          .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
          .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
          .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
          .enable_blend = blended}};
  const SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertex,
      .fragment_shader = fragment,
      .vertex_input_state = {.vertex_buffer_descriptions = &buffer_description,
                             .num_vertex_buffers = 1,
                             .vertex_attributes = attributes.data(),
                             .num_vertex_attributes =
                                 static_cast<Uint32>(attributes.size())},
      .primitive_type = topology == graphics::PrimitiveTopology::triangle_strip
                            ? SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP
                            : SDL_GPU_PRIMITIVETYPE_LINELIST,
      .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                           .cull_mode = SDL_GPU_CULLMODE_NONE,
                           .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE},
      .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
      .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                              .enable_depth_test = true,
                              .enable_depth_write = !blended},
      .target_info = {.color_target_descriptions = &target,
                      .num_color_targets = 1,
                      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                      .has_depth_stencil_target = true}};
  auto *pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  return pipeline;
}

struct BufferUpload {
  SDL_GPUTransferBuffer *transfer{};
  SDL_GPUBuffer *buffer{};
  Uint32 size{};
};

struct TextureUpload {
  SDL_GPUTransferBuffer *transfer{};
  SDL_GPUTexture *texture{};
  Uint32 width{};
  Uint32 height{};
};

[[nodiscard]] SDL_GPUTransferBuffer *
make_upload_transfer(SDL_GPUDevice *device, const void *bytes, Uint32 size) {
  const SDL_GPUTransferBufferCreateInfo info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};
  auto *transfer = SDL_CreateGPUTransferBuffer(device, &info);
  if (transfer == nullptr)
    return nullptr;
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return nullptr;
  }
  std::memcpy(mapped, bytes, size);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  return transfer;
}

[[nodiscard]] bool
upload_overlay_retail_textures(SDL_GPUDevice *device,
                               const ui::RetailUiTextureSet &source,
                               GpuOverlay &result) {
  constexpr std::size_t required_role_count =
      static_cast<std::size_t>(ui::RetailUiTextureRole::arrow_down) + 1U;
  const auto images = source.textures();
  // Until the retail UI-picture resource join is recovered, callers supply no
  // UI images. Never substitute generated pixels for that missing evidence.
  if (images.empty())
    return true;
  if (images.size() != required_role_count)
    return false;
  std::vector<TextureUpload> uploads;
  uploads.reserve(images.size());
  result.retail_textures.reserve(images.size());
  std::array<bool, required_role_count> seen{};
  const auto release_transfers = [&]() {
    for (const auto &upload : uploads)
      if (upload.transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, upload.transfer);
  };
  for (const auto &image : images) {
    const auto role_index = static_cast<std::size_t>(image.role);
    const auto width = image.mip_zero.width;
    const auto height = image.mip_zero.height;
    const std::uint64_t byte_count =
        static_cast<std::uint64_t>(width) * height * 4U;
    if (role_index >= seen.size() || seen[role_index] || byte_count == 0 ||
        byte_count > std::numeric_limits<Uint32>::max() ||
        image.mip_zero.pixels.size() != byte_count) {
      release_transfers();
      return false;
    }
    seen[role_index] = true;
    const SDL_GPUTextureCreateInfo info{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1};
    auto *texture = SDL_CreateGPUTexture(device, &info);
    auto *transfer = make_upload_transfer(device, image.mip_zero.pixels.data(),
                                          static_cast<Uint32>(byte_count));
    if (texture == nullptr || transfer == nullptr) {
      if (texture != nullptr)
        SDL_ReleaseGPUTexture(device, texture);
      if (transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, transfer);
      release_transfers();
      return false;
    }
    result.retail_textures.push_back({image.role, texture});
    uploads.push_back({transfer, texture, width, height});
  }
  SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy =
      command == nullptr ? nullptr : SDL_BeginGPUCopyPass(command);
  if (copy == nullptr) {
    if (command != nullptr)
      SDL_CancelGPUCommandBuffer(command);
    release_transfers();
    return false;
  }
  for (const auto &upload : uploads) {
    const SDL_GPUTextureTransferInfo from{.transfer_buffer = upload.transfer,
                                          .pixels_per_row = upload.width,
                                          .rows_per_layer = upload.height};
    const SDL_GPUTextureRegion to{.texture = upload.texture,
                                  .w = upload.width,
                                  .h = upload.height,
                                  .d = 1};
    SDL_UploadToGPUTexture(copy, &from, &to, false);
  }
  SDL_EndGPUCopyPass(copy);
  const bool uploaded =
      SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(device);
  release_transfers();
  return uploaded;
}

[[nodiscard]] bool upload_startup_images(
    SDL_GPUDevice *device, const graphics::StartupGraphicsAsset &source,
    GpuStartupImages &result) {
  const auto &images = source.images();
  if (images.size() != graphics::startup_graphics_image_count)
    return false;

  std::size_t aggregate_bytes = 0;
  std::vector<TextureUpload> uploads;
  uploads.reserve(images.size());
  result.images.reserve(images.size());
  const auto release_transfers = [&]() {
    for (const auto &upload : uploads)
      if (upload.transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, upload.transfer);
  };

  for (std::size_t index = 0; index < images.size(); ++index) {
    const auto &image = images[index];
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (images[previous].catalog_image_index == image.catalog_image_index ||
          images[previous].texture_id == image.texture_id) {
        release_transfers();
        return false;
      }
    }
    const auto width = image.mip_zero.width;
    const auto height = image.mip_zero.height;
    const auto byte_count = static_cast<std::uint64_t>(width) * height * 4U;
    if (width == 0 || height == 0 || byte_count == 0 ||
        byte_count > std::numeric_limits<Uint32>::max() ||
        byte_count > graphics::startup_graphics_decoded_byte_budget ||
        aggregate_bytes > graphics::startup_graphics_decoded_byte_budget -
                              static_cast<std::size_t>(byte_count) ||
        image.mip_zero.pixels.size() != byte_count) {
      release_transfers();
      return false;
    }
    aggregate_bytes += static_cast<std::size_t>(byte_count);

    const SDL_GPUTextureCreateInfo info{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1};
    auto *texture = SDL_CreateGPUTexture(device, &info);
    if (texture == nullptr) {
      release_transfers();
      return false;
    }
    result.images.push_back(
        {image.catalog_image_index, image.texture_id, texture});
    auto *transfer = make_upload_transfer(
        device, image.mip_zero.pixels.data(), static_cast<Uint32>(byte_count));
    if (transfer == nullptr) {
      release_transfers();
      return false;
    }
    uploads.push_back({transfer, texture, width, height});
  }

  SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy =
      command == nullptr ? nullptr : SDL_BeginGPUCopyPass(command);
  if (copy == nullptr) {
    if (command != nullptr)
      SDL_CancelGPUCommandBuffer(command);
    release_transfers();
    return false;
  }
  for (const auto &upload : uploads) {
    const SDL_GPUTextureTransferInfo from{.transfer_buffer = upload.transfer,
                                          .pixels_per_row = upload.width,
                                          .rows_per_layer = upload.height};
    const SDL_GPUTextureRegion to{.texture = upload.texture,
                                  .w = upload.width,
                                  .h = upload.height,
                                  .d = 1};
    SDL_UploadToGPUTexture(copy, &from, &to, false);
  }
  SDL_EndGPUCopyPass(copy);
  const bool uploaded =
      SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(device);
  release_transfers();
  return uploaded;
}

[[nodiscard]] bool upload_scene(SDL_GPUDevice *device, SDL_Window *window,
                                const graphics::SceneGpuPlan &source,
                                GpuScene &result) {
  result.meshes.resize(source.meshes.size());
  result.textures.resize(source.textures.size());
  std::vector<BufferUpload> buffers;
  std::vector<TextureUpload> textures;
  auto release_transfers = [&]() {
    for (const auto &upload : buffers)
      if (upload.transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, upload.transfer);
    for (const auto &upload : textures)
      if (upload.transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, upload.transfer);
  };
  for (std::size_t index = 0; index < source.meshes.size(); ++index) {
    const auto &mesh = source.meshes[index];
    const auto vertex_bytes = mesh.vertices.size() * sizeof(mesh.vertices[0]);
    const auto index_bytes = mesh.indices.size() * sizeof(mesh.indices[0]);
    if (vertex_bytes == 0 || index_bytes == 0 ||
        vertex_bytes > std::numeric_limits<Uint32>::max() ||
        index_bytes > std::numeric_limits<Uint32>::max()) {
      release_transfers();
      return false;
    }
    const SDL_GPUBufferCreateInfo vertex_info{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = static_cast<Uint32>(vertex_bytes)};
    const SDL_GPUBufferCreateInfo index_info{
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = static_cast<Uint32>(index_bytes)};
    auto &gpu_mesh = result.meshes[index];
    gpu_mesh.vertex_buffer = SDL_CreateGPUBuffer(device, &vertex_info);
    gpu_mesh.index_buffer = SDL_CreateGPUBuffer(device, &index_info);
    auto *vertex_transfer = make_upload_transfer(
        device, mesh.vertices.data(), static_cast<Uint32>(vertex_bytes));
    auto *index_transfer = make_upload_transfer(
        device, mesh.indices.data(), static_cast<Uint32>(index_bytes));
    if (gpu_mesh.vertex_buffer == nullptr || gpu_mesh.index_buffer == nullptr ||
        vertex_transfer == nullptr || index_transfer == nullptr) {
      if (vertex_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
      if (index_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, index_transfer);
      release_transfers();
      return false;
    }
    buffers.push_back({vertex_transfer, gpu_mesh.vertex_buffer,
                       static_cast<Uint32>(vertex_bytes)});
    buffers.push_back({index_transfer, gpu_mesh.index_buffer,
                       static_cast<Uint32>(index_bytes)});
  }
  const auto create_texture = [&](Uint32 width, Uint32 height,
                                  const std::uint8_t *rgba,
                                  SDL_GPUTexture *&destination) {
    const auto byte_count = static_cast<std::uint64_t>(width) * height * 4U;
    if (byte_count == 0 || byte_count > std::numeric_limits<Uint32>::max())
      return false;
    const SDL_GPUTextureCreateInfo info{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1};
    destination = SDL_CreateGPUTexture(device, &info);
    auto *transfer =
        make_upload_transfer(device, rgba, static_cast<Uint32>(byte_count));
    if (destination == nullptr || transfer == nullptr) {
      if (transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, transfer);
      return false;
    }
    textures.push_back({transfer, destination, width, height});
    return true;
  };
  for (std::size_t index = 0; index < source.textures.size(); ++index) {
    const auto &texture = source.textures[index];
    if (!create_texture(texture.width, texture.height, texture.rgba8.data(),
                        result.textures[index])) {
      release_transfers();
      return false;
    }
  }
  constexpr std::array<std::uint8_t, 4> white{255, 255, 255, 255};
  if (!create_texture(1, 1, white.data(), result.white_texture)) {
    release_transfers();
    return false;
  }
  const SDL_GPUSamplerCreateInfo sampler_info{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT};
  result.sampler = SDL_CreateGPUSampler(device, &sampler_info);
  result.triangle_opaque = create_scene_pipeline(
      device, window, graphics::PrimitiveTopology::triangle_strip, false);
  result.triangle_blended = create_scene_pipeline(
      device, window, graphics::PrimitiveTopology::triangle_strip, true);
  result.line_opaque = create_scene_pipeline(
      device, window, graphics::PrimitiveTopology::line_list, false);
  result.line_blended = create_scene_pipeline(
      device, window, graphics::PrimitiveTopology::line_list, true);
  if (result.sampler == nullptr || result.triangle_opaque == nullptr ||
      result.triangle_blended == nullptr || result.line_opaque == nullptr ||
      result.line_blended == nullptr) {
    release_transfers();
    return false;
  }

  SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy =
      command == nullptr ? nullptr : SDL_BeginGPUCopyPass(command);
  if (copy == nullptr) {
    if (command != nullptr)
      SDL_CancelGPUCommandBuffer(command);
    release_transfers();
    return false;
  }
  for (const auto &upload : buffers) {
    const SDL_GPUTransferBufferLocation from{.transfer_buffer =
                                                 upload.transfer};
    const SDL_GPUBufferRegion to{.buffer = upload.buffer, .size = upload.size};
    SDL_UploadToGPUBuffer(copy, &from, &to, false);
  }
  for (const auto &upload : textures) {
    const SDL_GPUTextureTransferInfo from{.transfer_buffer = upload.transfer,
                                          .pixels_per_row = upload.width,
                                          .rows_per_layer = upload.height};
    const SDL_GPUTextureRegion to{.texture = upload.texture,
                                  .w = upload.width,
                                  .h = upload.height,
                                  .d = 1};
    SDL_UploadToGPUTexture(copy, &from, &to, false);
  }
  SDL_EndGPUCopyPass(copy);
  const bool uploaded =
      SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(device);
  release_transfers();
  return uploaded;
}

[[nodiscard]] bool ensure_scene_depth(SDL_GPUDevice *device, Uint32 width,
                                      Uint32 height, GpuScene &scene) {
  if (width == 0 || height == 0)
    return true;
  if (scene.depth != nullptr && scene.depth_width == width &&
      scene.depth_height == height)
    return true;
  if (!SDL_WaitForGPUIdle(device))
    return false;
  if (scene.depth != nullptr)
    SDL_ReleaseGPUTexture(device, scene.depth);
  scene.depth = nullptr;
  const SDL_GPUTextureCreateInfo info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1};
  scene.depth = SDL_CreateGPUTexture(device, &info);
  if (scene.depth == nullptr) {
    scene.depth_width = 0;
    scene.depth_height = 0;
    return false;
  }
  scene.depth_width = width;
  scene.depth_height = height;
  return true;
}

} // namespace

RuntimeResult
run_sdl_gpu_runtime(const StartupWindow &startup_window, Mode mode,
                    const graphics::SceneGpuPlan &scene,
                    const graphics::StartupGraphicsAsset &startup_graphics,
                    const ui::RetailUiFontSet &ui_fonts,
                    const ui::RetailUiTextureSet &ui_textures,
                    std::size_t frame_limit, bool show_graphics_menu,
                    const std::filesystem::path &screenshot_path) {
  if (ui_fonts.fonts.empty())
    return failure("retail UI font set is empty");
  try {
    graphics::validate_scene_gpu_plan(scene);
  } catch (const std::exception &error) {
    return {.success = false,
            .message = std::string("scene GPU plan validation failed: ") +
                       error.what()};
  }
  SDL_Window *window = startup_window.get();
  if (window == nullptr)
    return {.success = false, .message = "Startup window is missing"};
  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    return failure("SDL gamepad initialization failed");
  const GamepadSession gamepad_session;
  // SDL's software window surface and a 3D swapchain cannot coexist. Release
  // the splash surface on its creator thread before claiming this SAME window.
  if (SDL_WindowHasSurface(window) && !SDL_DestroyWindowSurface(window))
    return failure("Startup window surface release failed");
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
    return result;
  }
  GpuScene gpu;
  if (!upload_scene(device, window, scene, gpu)) {
    const auto result = failure("scene GPU upload failed");
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }
  GpuOverlay overlay;
  if (!create_overlay(device, window, ui_fonts, overlay)) {
    const auto result = failure("graphics overlay GPU upload failed");
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }
  if (!upload_overlay_retail_textures(device, ui_textures, overlay)) {
    const auto result = failure("retail UI texture GPU upload failed");
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }
  GpuStartupImages gpu_startup;
  if (!upload_startup_images(device, startup_graphics, gpu_startup)) {
    const auto result = failure("startup graphics image GPU upload failed");
    release_startup_images(device, gpu_startup);
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
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

  RuntimeResult result{
      .success = true,
      .message =
          std::string("Renderer: SDL GPU/") + SDL_GetGPUDeviceDriver(device) +
          " (source-only diagnostic scene: " +
          std::to_string(scene.meshes.size()) + " meshes, " +
          std::to_string(scene.instances.size()) + " instances, " +
          std::to_string(scene.draws.size()) + " draws; " +
          std::to_string(gpu_startup.images.size()) +
          " startup graphics images uploaded, not rendered; " +
          std::to_string(ui_fonts.fonts.size()) + " retail UI fonts loaded)"};
  constexpr std::array<float, 32> matrices{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,
                                           0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool running = true;
  bool screenshot_captured = screenshot_path.empty();
  std::size_t frames = 0;
  SdlMenuGamepad menu_gamepad{
      SDL_GetWindowID(window),
      (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0};
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      const auto gamepad_key = menu_gamepad.handle_event(
          event, menu.phase() != ui::GraphicsMenuPhase::closed);
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
        continue;
      }
      std::optional<ui::GraphicsMenuKey> translated_key = gamepad_key;
      bool pressed = gamepad_key.has_value();
      bool repeated = false;
      if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        translated_key = menu_key(event.key.key);
        pressed = event.type == SDL_EVENT_KEY_DOWN;
        repeated = event.key.repeat;
      }
      if (translated_key) {
        const auto key = *translated_key;
        ui::GraphicsMenuEffect effect = ui::GraphicsMenuEffect::none;
        if (menu.phase() == ui::GraphicsMenuPhase::confirming &&
            pressed && !repeated &&
            (key == ui::GraphicsMenuKey::enter ||
             key == ui::GraphicsMenuKey::space)) {
          effect = menu.confirm();
        } else {
          effect = menu.handle_key(key, pressed, repeated);
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
    if (swapchain != nullptr &&
        !ensure_scene_depth(device, swapchain_width, swapchain_height, gpu)) {
      result = failure("scene depth target creation failed");
      SDL_SubmitGPUCommandBuffer(command);
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      break;
    }
    const auto draw_list = ui::build_graphics_menu_draw_list(
        menu, {swapchain_width, swapchain_height}, ui::GraphicsClock::now());
    const auto overlay_batch =
        draw_list.status == ui::UiBuildStatus::ok
            ? build_overlay_batch(draw_list, overlay.font)
            : OverlayBatch{};
    SDL_GPUTransferBuffer *overlay_transfer = nullptr;
    SDL_GPUTransferBuffer *overlay_atlas_transfer = nullptr;
    const auto release_overlay_transfers = [&]() {
      if (overlay_transfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, overlay_transfer);
        overlay_transfer = nullptr;
      }
      if (overlay_atlas_transfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, overlay_atlas_transfer);
        overlay_atlas_transfer = nullptr;
      }
    };
    if (!upload_overlay(device, command, overlay_batch, overlay,
                        overlay_transfer, overlay_atlas_transfer)) {
      result = failure("graphics overlay frame upload failed");
      SDL_SubmitGPUCommandBuffer(command);
      SDL_WaitForGPUIdle(device);
      release_overlay_transfers();
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
      const SDL_GPUDepthStencilTargetInfo depth_target{
          .texture = gpu.depth,
          .clear_depth = 1.0F,
          .load_op = SDL_GPU_LOADOP_CLEAR,
          .store_op = SDL_GPU_STOREOP_DONT_CARE,
          .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
          .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
          .cycle = true};
      SDL_GPURenderPass *pass =
          SDL_BeginGPURenderPass(command, &target, 1, &depth_target);
      if (pass == nullptr) {
        result = failure("SDL GPU render-pass creation failed");
        SDL_SubmitGPUCommandBuffer(command);
        SDL_WaitForGPUIdle(device);
        release_overlay_transfers();
        break;
      }
      std::optional<std::size_t> bound_mesh;
      std::optional<std::size_t> bound_instance;
      SDL_GPUGraphicsPipeline *bound_pipeline = nullptr;
      SDL_GPUTexture *bound_texture = nullptr;
      for (const auto &draw : scene.draws) {
        if (draw.depth_policy == graphics::SceneDepthPolicy::no_draw)
          continue;
        SDL_GPUGraphicsPipeline *pipeline = nullptr;
        if (draw.topology == graphics::PrimitiveTopology::triangle_strip)
          pipeline =
              draw.blend_enabled ? gpu.triangle_blended : gpu.triangle_opaque;
        else
          pipeline = draw.blend_enabled ? gpu.line_blended : gpu.line_opaque;
        if (pipeline != bound_pipeline) {
          SDL_BindGPUGraphicsPipeline(pass, pipeline);
          bound_pipeline = pipeline;
        }
        if (bound_mesh != draw.mesh_index) {
          const auto &mesh = gpu.meshes[draw.mesh_index];
          const SDL_GPUBufferBinding vb{.buffer = mesh.vertex_buffer,
                                        .offset = 0};
          const SDL_GPUBufferBinding ib{.buffer = mesh.index_buffer,
                                        .offset = 0};
          SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
          SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
          bound_mesh = draw.mesh_index;
        }
        SDL_GPUTexture *texture = draw.texture_index.has_value()
                                      ? gpu.textures[*draw.texture_index]
                                      : gpu.white_texture;
        if (texture != bound_texture) {
          const SDL_GPUTextureSamplerBinding binding{.texture = texture,
                                                     .sampler = gpu.sampler};
          SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
          bound_texture = texture;
        }
        if (bound_instance != draw.instance_index) {
          const auto scene_uniform = graphics::make_scene_diagnostic_matrices(
              scene, draw.instance_index, swapchain_width, swapchain_height);
          std::array<float, 32> packed{};
          std::copy(scene_uniform.projection_view.begin(),
                    scene_uniform.projection_view.end(), packed.begin());
          std::copy(scene_uniform.model.begin(), scene_uniform.model.end(),
                    packed.begin() + 16);
          SDL_PushGPUVertexUniformData(command, 0, packed.data(),
                                       sizeof(packed));
          bound_instance = draw.instance_index;
        }
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
          SDL_WaitForGPUIdle(device);
          release_overlay_transfers();
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
        SDL_GPUTexture *bound_overlay_texture = nullptr;
        for (const auto &draw : overlay_batch.draws) {
          SDL_Rect scissor = full_scissor;
          if (draw.clip.has_value()) {
            scissor = {static_cast<int>(std::floor(draw.clip->x)),
                       static_cast<int>(std::floor(draw.clip->y)),
                       static_cast<int>(std::ceil(draw.clip->width)),
                       static_cast<int>(std::ceil(draw.clip->height))};
          }
          SDL_SetGPUScissor(overlay_pass, &scissor);
          SDL_GPUTexture *texture = overlay.atlas;
          if (draw.texture_role.has_value()) {
            const auto found = std::find_if(
                overlay.retail_textures.begin(), overlay.retail_textures.end(),
                [&](const GpuUiTexture &candidate) {
                  return candidate.role == *draw.texture_role;
                });
            if (found == overlay.retail_textures.end()) {
              result = {.success = false,
                        .message = "graphics overlay references a missing "
                                   "retail UI texture"};
              break;
            }
            texture = found->texture;
          }
          if (texture != bound_overlay_texture) {
            const SDL_GPUTextureSamplerBinding binding{
                .texture = texture, .sampler = overlay.sampler};
            SDL_BindGPUFragmentSamplers(overlay_pass, 0, &binding, 1);
            bound_overlay_texture = texture;
          }
          SDL_DrawGPUPrimitives(overlay_pass,
                                static_cast<Uint32>(draw.vertex_count), 1,
                                static_cast<Uint32>(draw.first_vertex), 0);
        }
        SDL_EndGPURenderPass(overlay_pass);
        if (!result.success) {
          SDL_SubmitGPUCommandBuffer(command);
          SDL_WaitForGPUIdle(device);
          release_overlay_transfers();
          break;
        }
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
          SDL_WaitForGPUIdle(device);
          release_overlay_transfers();
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
      release_overlay_transfers();
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
      if (!result.success) {
        release_overlay_transfers();
        break;
      }
    }
    if (overlay_transfer != nullptr) {
      SDL_WaitForGPUIdle(device);
    }
    release_overlay_transfers();
    ++frames;
    if (frame_limit != 0 && frames >= frame_limit)
      running = false;
  }
  SDL_WaitForGPUIdle(device);
  release_overlay(device, overlay);
  release_startup_images(device, gpu_startup);
  release_scene(device, gpu);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyGPUDevice(device);
  return result;
}

} // namespace off::platform
