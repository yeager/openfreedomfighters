#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/platform/runtime_presentation_settings.hpp"
#include "off/graphics/scene_instance_history.hpp"
#include "off/graphics/temporal_resolve_baseline.hpp"
#include "off/platform/sdl_intro_renderer.hpp"
#include "off/platform/intro_preview_diagnostic.hpp"
#include "off/platform/sdl_locale.hpp"
#include "off/platform/sdl_menu_gamepad.hpp"
#include "off/platform/sdl_menu_keyboard.hpp"
#include "off/settings/upscaler_runtime.hpp"
#include "off/settings/graphics_settings_store.hpp"
#include "off/ui/graphics_menu_draw.hpp"
#include "off/ui/graphics_menu_pointer.hpp"
#include "off/ui/font_run_layout.hpp"

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

// The scene may render below or above presentation resolution. UI remains on
// the presentation target and is therefore never softened by render scaling.
struct GpuRenderScaleTarget {
  SDL_GPUTexture *texture{nullptr};
  Uint32 width{0};
  Uint32 height{0};
  SDL_GPUTextureFormat format{SDL_GPU_TEXTUREFORMAT_INVALID};
};

// Diagnostic-scene-only portable temporal baseline. It owns actual SDL GPU
// surfaces and a two-sample resolve pass; it is not a vendor upscaler and is
// never admitted for normal startup/world rendering.
struct GpuTemporalResolveBaseline {
  GpuRenderScaleTarget current;
  std::array<GpuRenderScaleTarget, 2> history;
  GpuRenderScaleTarget motion;
  GpuRenderScaleTarget exposure;
  GpuRenderScaleTarget reactive;
  SDL_GPUBuffer *vertices{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *pipeline{nullptr};
  graphics::TemporalResolveBaseline contract;
};

class SceneInstanceHistorySubmissionGuard {
public:
  explicit SceneInstanceHistorySubmissionGuard(
      graphics::SceneInstanceHistoryLifecycle &history)
      : history_(history) {}
  SceneInstanceHistorySubmissionGuard(const SceneInstanceHistorySubmissionGuard &) = delete;
  SceneInstanceHistorySubmissionGuard &operator=(
      const SceneInstanceHistorySubmissionGuard &) = delete;
  ~SceneInstanceHistorySubmissionGuard() {
    if (submission_)
      history_.cancel_submission();
  }

  [[nodiscard]] bool begin(
      std::span<const graphics::SceneInstanceSubmissionTransform> instances) {
    submission_ = history_.begin_submission(instances);
    return submission_.has_value();
  }

  [[nodiscard]] bool commit() noexcept {
    if (!submission_ || !history_.commit_submission())
      return false;
    submission_.reset();
    return true;
  }

private:
  graphics::SceneInstanceHistoryLifecycle &history_;
  std::optional<std::vector<graphics::SceneInstanceSubmissionTransform>> submission_;
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

struct GpuOverlayFont {
  SDL_IOStream *stream{nullptr};
  TTF_Font *font{nullptr};
};

struct GpuOverlay {
  SDL_GPUBuffer *vertex_buffer{nullptr};
  SDL_GPUTexture *atlas{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  SDL_GPUGraphicsPipeline *pipeline{nullptr};
  std::vector<GpuOverlayFont> fonts;
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

void release_render_scale_target(SDL_GPUDevice *device,
                                 GpuRenderScaleTarget &target) {
  if (target.texture != nullptr)
    SDL_ReleaseGPUTexture(device, target.texture);
  target = {};
}

[[nodiscard]] bool ensure_render_scale_target(
    SDL_GPUDevice *device, Uint32 width, Uint32 height,
    SDL_GPUTextureFormat format, GpuRenderScaleTarget &target) {
  if (width == 0 || height == 0 || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    return false;
  if (target.texture != nullptr && target.width == width &&
      target.height == height && target.format == format)
    return true;
  if (target.texture != nullptr) {
    if (!SDL_WaitForGPUIdle(device))
      return false;
    release_render_scale_target(device, target);
  }
  const SDL_GPUTextureCreateInfo info{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = format,
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
               SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1};
  target.texture = SDL_CreateGPUTexture(device, &info);
  if (target.texture == nullptr)
    return false;
  target.width = width;
  target.height = height;
  target.format = format;
  return true;
}

void release_overlay(SDL_GPUDevice *device, GpuOverlay &overlay) {
  for (auto &font : overlay.fonts)
    if (font.font != nullptr)
      TTF_CloseFont(font.font);
  for (auto &font : overlay.fonts)
    if (font.stream != nullptr)
      SDL_CloseIO(font.stream);
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

void release_temporal_resolve_baseline(SDL_GPUDevice *device,
                                       GpuTemporalResolveBaseline &baseline) {
  if (baseline.pipeline != nullptr)
    SDL_ReleaseGPUGraphicsPipeline(device, baseline.pipeline);
  if (baseline.sampler != nullptr)
    SDL_ReleaseGPUSampler(device, baseline.sampler);
  if (baseline.vertices != nullptr)
    SDL_ReleaseGPUBuffer(device, baseline.vertices);
  release_render_scale_target(device, baseline.reactive);
  release_render_scale_target(device, baseline.exposure);
  release_render_scale_target(device, baseline.motion);
  for (auto &target : baseline.history)
    release_render_scale_target(device, target);
  release_render_scale_target(device, baseline.current);
  baseline = {};
}

[[nodiscard]] bool create_temporal_resolve_pipeline(
    SDL_GPUDevice *device, SDL_Window *window,
    GpuTemporalResolveBaseline &baseline) {
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
  const SDL_GPUVertexBufferDescription description{
      .slot = 0, .pitch = sizeof(PreviewVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0};
  const std::array attributes{
      SDL_GPUVertexAttribute{.location = 0, .buffer_slot = 0,
                              .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                              .offset = offsetof(PreviewVertex, position)},
      SDL_GPUVertexAttribute{.location = 1, .buffer_slot = 0,
                              .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                              .offset = offsetof(PreviewVertex, color)},
      SDL_GPUVertexAttribute{.location = 2, .buffer_slot = 0,
                              .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                              .offset = offsetof(PreviewVertex, uv)}};
  const SDL_GPUColorTargetDescription target{
      .format = SDL_GetGPUSwapchainTextureFormat(device, window),
      .blend_state = {.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                      .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                      .color_blend_op = SDL_GPU_BLENDOP_ADD,
                      .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                      .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                      .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                      .enable_blend = true}};
  const SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertex,
      .fragment_shader = fragment,
      .vertex_input_state = {.vertex_buffer_descriptions = &description,
                             .num_vertex_buffers = 1,
                             .vertex_attributes = attributes.data(),
                             .num_vertex_attributes = static_cast<Uint32>(attributes.size())},
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                           .cull_mode = SDL_GPU_CULLMODE_NONE,
                           .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE},
      .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
      .target_info = {.color_target_descriptions = &target,
                      .num_color_targets = 1,
                      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
                      .has_depth_stencil_target = false}};
  baseline.pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  return baseline.pipeline != nullptr;
}

[[nodiscard]] bool ensure_temporal_resolve_baseline(
    SDL_GPUDevice *device, SDL_Window *window, Uint32 output_width,
    Uint32 output_height, Uint32 internal_width, Uint32 internal_height,
    SDL_GPUTextureFormat format, GpuTemporalResolveBaseline &baseline) {
  if (output_width == 0 || output_height == 0 || internal_width == 0 ||
      internal_height == 0 || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    return false;
  const bool history_recreated = baseline.contract.configure(
      {output_width, output_height, static_cast<std::uint32_t>(format)});
  if (!ensure_render_scale_target(device, internal_width, internal_height,
                                  format, baseline.current) ||
      !ensure_render_scale_target(device, output_width, output_height, format,
                                  baseline.history[0]) ||
      !ensure_render_scale_target(device, output_width, output_height, format,
                                  baseline.history[1]) ||
      !ensure_render_scale_target(device, internal_width, internal_height,
                                  format, baseline.motion) ||
      !ensure_render_scale_target(device, internal_width, internal_height,
                                  format, baseline.exposure) ||
      !ensure_render_scale_target(device, internal_width, internal_height,
                                  format, baseline.reactive))
    return false;
  if (history_recreated)
    baseline.contract.invalidate();
  if (baseline.vertices == nullptr) {
    const SDL_GPUBufferCreateInfo info{.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                       .size = static_cast<Uint32>(
                                           sizeof(PreviewVertex) * 12U)};
    baseline.vertices = SDL_CreateGPUBuffer(device, &info);
  }
  if (baseline.sampler == nullptr) {
    const SDL_GPUSamplerCreateInfo info{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
    baseline.sampler = SDL_CreateGPUSampler(device, &info);
  }
  return baseline.vertices != nullptr && baseline.sampler != nullptr &&
         (baseline.pipeline != nullptr ||
          create_temporal_resolve_pipeline(device, window, baseline));
}

[[nodiscard]] bool create_overlay(SDL_GPUDevice *device, SDL_Window *window,
                                  const ui::RetailUiFontSet &fonts,
                                  GpuOverlay &result) {
  if (fonts.fonts.empty() || !TTF_Init())
    return false;
  result.ttf_initialized = true;
  result.fonts.reserve(fonts.fonts.size());
  for (const auto &source : fonts.fonts) {
    auto stream = SDL_IOFromConstMem(source.sfnt.data(), source.sfnt.size());
    auto *font = stream == nullptr ? nullptr : TTF_OpenFontIO(stream, false, 24.0F);
    result.fonts.push_back({stream, font});
    if (font == nullptr) return false;
  }
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
build_overlay_batch(const ui::GraphicsMenuDrawList &list,
                    const ui::RetailUiFontSet &sources,
                    std::span<const GpuOverlayFont> fonts) {
  OverlayBatch batch;
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
    const auto runs = ui::select_font_runs_for_utf8(sources, command.text);
    if (!runs) {
      batch.valid = false;
      continue;
    }
    const auto layer = static_cast<std::size_t>(command.layer);
    std::vector<SDL_Surface *> run_surfaces;
    std::vector<ui::FontRunRasterMetrics> metrics;
    run_surfaces.reserve(runs->size());
    metrics.reserve(runs->size());
    for (const auto &run : *runs) {
      if(run.font_index>=fonts.size() || fonts[run.font_index].font==nullptr ||
          !TTF_SetFontSize(fonts[run.font_index].font,24.0F*list.ui_scale)) {
        batch.valid=false;
        break;
      }
      SDL_Surface* rendered=TTF_RenderText_Blended(fonts[run.font_index].font,
          command.text.data()+run.byte_offset,run.byte_length,{255,255,255,255});
      SDL_Surface* surface=rendered==nullptr?nullptr:SDL_ConvertSurface(rendered,SDL_PIXELFORMAT_RGBA32);
      if(rendered!=nullptr) SDL_DestroySurface(rendered);
      if(surface==nullptr) {batch.valid=false;break;}
      const int ascent = TTF_GetFontAscent(fonts[run.font_index].font);
      if(surface->w<=0 || surface->h<=0 || ascent < 0) {
        batch.valid=false;SDL_DestroySurface(surface);break;
      }
      if(surface->w>=static_cast<int>(overlay_atlas_width) || surface->h>=static_cast<int>(overlay_atlas_height)) {
        batch.valid=false;SDL_DestroySurface(surface);break;
      }
      metrics.push_back({surface->w, surface->h, ascent});
      run_surfaces.push_back(surface);
    }
    const auto placements = batch.valid
        ? ui::layout_ltr_font_runs(command.x, command.y, metrics)
        : std::nullopt;
    if (!placements || placements->size() != run_surfaces.size())
      batch.valid = false;
    for (std::size_t index = 0; batch.valid && index < run_surfaces.size(); ++index) {
      SDL_Surface *surface = run_surfaces[index];
      const Uint32 width=static_cast<Uint32>(surface->w),height=static_cast<Uint32>(surface->h);
      if(cursor_x+width+1U>overlay_atlas_width) {cursor_x=1;cursor_y+=shelf_height+1U;shelf_height=0;}
      if(cursor_y+height+1U<=overlay_atlas_height && width+2U<=overlay_atlas_width) {
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
                          {(*placements)[index].x, (*placements)[index].y,
                           (*placements)[index].width, (*placements)[index].height},
                          command.clip, command.color, list.target, u0, v0, u1,
                          v1);
      if (!piece.vertices.empty())
        text_pieces[layer].push_back(std::move(piece));
      cursor_x += width + 1U;
      shelf_height = std::max(shelf_height, height);
    } else {
      batch.valid = false;
    }
    }
    for (SDL_Surface *surface : run_surfaces)
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
  Uint32 mip_level{};
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

template<class Images>
[[nodiscard]] bool upload_picture_images(
    SDL_GPUDevice *device, const Images &images, std::size_t byte_budget,
    GpuStartupImages &result) {
  if (images.empty())
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
        byte_count > byte_budget ||
        aggregate_bytes > byte_budget -
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
  const auto create_texture = [&](const graphics::SceneGpuTexture &source,
                                  SDL_GPUTexture *&destination) {
    if (source.mips.empty() ||
        source.mips.size() > std::numeric_limits<Uint32>::max())
      return false;
    const auto width = source.mips.front().width;
    const auto height = source.mips.front().height;
    const SDL_GPUTextureCreateInfo info{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = static_cast<Uint32>(source.mips.size()),
        .sample_count = SDL_GPU_SAMPLECOUNT_1};
    destination = SDL_CreateGPUTexture(device, &info);
    if (destination == nullptr)
      return false;
    for (std::size_t mip_level = 0; mip_level < source.mips.size(); ++mip_level) {
      const auto &mip = source.mips[mip_level];
      const auto byte_count =
          static_cast<std::uint64_t>(mip.width) * mip.height * 4U;
      if (byte_count == 0 || byte_count > std::numeric_limits<Uint32>::max())
        return false;
      auto *transfer = make_upload_transfer(device, mip.rgba8.data(),
                                            static_cast<Uint32>(byte_count));
      if (transfer == nullptr)
        return false;
      textures.push_back({transfer, destination, mip.width, mip.height,
                          static_cast<Uint32>(mip_level)});
    }
    return true;
  };
  for (std::size_t index = 0; index < source.textures.size(); ++index) {
    const auto &texture = source.textures[index];
    if (!create_texture(texture, result.textures[index])) {
      release_transfers();
      return false;
    }
  }
  constexpr std::array<std::uint8_t, 4> white{255, 255, 255, 255};
  const graphics::SceneGpuTexture white_texture{
      .mips = {{.width = 1, .height = 1, .rgba8 = {white.begin(), white.end()}}}};
  if (!create_texture(white_texture, result.white_texture)) {
    release_transfers();
    return false;
  }
  const SDL_GPUSamplerCreateInfo sampler_info{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
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
                                  .mip_level = upload.mip_level,
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
                    bool mode_explicitly_requested,
                    const std::filesystem::path &graphics_settings_path,
                    const graphics::SceneGpuPlan *scene,
                    const graphics::StartupGraphicsAsset &startup_graphics,
                    const ui::RetailUiFontSet &ui_fonts,
                    const ui::RetailUiTextureSet &ui_textures,
                    graphics::IntroRuntime *intro,
                    const graphics::IntroPreviewSnapshot *intro_preview_diagnostic,
                    std::size_t frame_limit, bool show_graphics_menu,
                    const std::filesystem::path &screenshot_path,
                    std::string_view explicit_locale,
                    bool startup_graphics_scene_diagnostic) {
  if (ui_fonts.fonts.empty())
    return failure("retail UI font set is empty");
  if ((scene != nullptr) == (intro != nullptr))
    return {.success = false,
            .message = "Select exactly one retained intro host or diagnostic scene"};
  try {
    if (scene)
      graphics::validate_scene_gpu_plan(*scene);
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
  // Preserve the host's language preference order unless an explicit locale
  // override was supplied.
  const auto platform_locale_storage = preferred_system_locales();
  std::vector<std::string_view> platform_locales;
  platform_locales.reserve(platform_locale_storage.size());
  for (const auto &locale : platform_locale_storage)
    platform_locales.push_back(locale);
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
  if (scene && !upload_scene(device, window, *scene, gpu)) {
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
  if (startup_graphics.images().size() != graphics::startup_graphics_image_count ||
      !upload_picture_images(device, startup_graphics.images(),
          graphics::startup_graphics_decoded_byte_budget, gpu_startup)) {
    const auto result = failure("startup graphics image GPU upload failed");
    release_startup_images(device, gpu_startup);
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }

  // These are the retained FF-Intro images, not the separate startup UI set.
  // Keep allocation/image identities through the entire window lifetime. Upload
  // is not scene admission: unresolved cut start must not become a fake draw.
  std::unique_ptr<SdlIntroRenderer> gpu_intro;
  try {
    if (intro)
      gpu_intro = std::make_unique<SdlIntroRenderer>(device, intro->resources().images());
  } catch (const std::exception& error) {
    const RuntimeResult result{false, std::string("intro renderer initialization failed: ") + error.what()};
    release_startup_images(device, gpu_startup);
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }
  if (intro_preview_diagnostic != nullptr && gpu_intro == nullptr) {
    const RuntimeResult result{false,
        "intro diagnostic requires retained intro image resources"};
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
  // No enhanced Modern+ presentation path is bound in this SDL GPU runtime
  // yet.  Keep the profile and its vendor options unavailable rather than
  // accepting a setting that would render through the plain native path.
  capabilities = negotiate_runtime_presentation_capabilities(capabilities,
                                                               false);
  // SDL GPU currently owns fixed render scaling only. Do not advertise a
  // temporal or vendor upscaler until this renderer has completed an actual
  // native submission binding for it.
  capabilities = settings::negotiate_upscaler_runtime_capabilities(
      capabilities, {});
  ui::GraphicsMenuSession menu{capabilities};
  // A malformed or unavailable preferences file is never repaired here. The
  // default request remains usable, while an explicit CLI profile is a
  // one-launch override rather than a stored mutation.
  const auto initial = settings::load_initial_graphics_settings(
      graphics_settings_path, mode, mode_explicitly_requested);
  const auto initial_resolution =
      settings::resolve_graphics_settings(initial, capabilities);
  const auto initial_setup = settings::initialize_graphics_settings(
      initial_resolution, [&](const settings::EffectiveGraphicsSettings &value) {
        return apply_graphics(device, window, value);
      });
  if (initial_setup != settings::InitialGraphicsSetup::ready) {
    const auto result = failure(initial_setup ==
                                        settings::InitialGraphicsSetup::apply_failed
                                    ? "initial graphics configuration failed"
                                    : "initial graphics configuration is invalid");
    gpu_intro.reset();
    release_startup_images(device, gpu_startup);
    release_overlay(device, overlay);
    release_scene(device, gpu);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    return result;
  }
  menu.set_confirmed(initial, *initial_resolution.effective);
  if (show_graphics_menu) {
    static_cast<void>(menu.handle_key(ui::GraphicsMenuKey::f10, true, false));
  }
  GpuRenderScaleTarget render_scale_target;
  GpuTemporalResolveBaseline temporal_baseline;
  graphics::SceneInstanceHistoryLifecycle scene_instance_history;
  std::vector<std::uint64_t> scene_instance_identities;
  std::optional<std::array<Uint32, 2>> scene_history_extent;
  if (scene != nullptr) {
    const auto initial_submission =
        graphics::make_scene_gpu_instance_submission(scene->instances);
    scene_instance_identities.reserve(initial_submission.size());
    for (const auto &instance : initial_submission)
      scene_instance_identities.push_back(instance.identity);
    if (!scene_instance_history.initialize(initial_submission)) {
      const auto result = failure("scene instance history initialization failed");
      release_render_scale_target(device, render_scale_target);
      gpu_intro.reset();
      release_startup_images(device, gpu_startup);
      release_overlay(device, overlay);
      release_scene(device, gpu);
      SDL_ReleaseWindowFromGPUDevice(device, window);
      SDL_DestroyGPUDevice(device);
      return result;
    }
  }

  RuntimeResult result{
      .success = true,
      .message =
          std::string("Renderer: SDL GPU/") + SDL_GetGPUDeviceDriver(device) +
          (scene
               ? " (explicit source-only diagnostic scene: " +
                     std::to_string(scene->meshes.size()) + " meshes, " +
                     std::to_string(scene->instances.size()) + " instances, " +
                     std::to_string(scene->draws.size()) + " draws; "
               : " (authored startup resources loaded; world rendering "
                 "pending; ") +
          std::to_string(gpu_startup.images.size()) +
          (startup_graphics_scene_diagnostic
               ? " startup graphics images represented by the diagnostic scene; "
               : " startup graphics images uploaded, not rendered; ") +
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
        translated_key = translate_menu_keyboard_event(
            event, SDL_GetWindowID(window),
            (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0);
        pressed = event.type == SDL_EVENT_KEY_DOWN;
        repeated = event.key.repeat;
      }
      // Deadline wins over every simultaneous keyboard, gamepad or pointer
      // action; rollback goes through the same effect handler exactly once.
      ui::GraphicsMenuEffect effect = menu.tick(ui::GraphicsClock::now());
      const bool expired = effect == ui::GraphicsMenuEffect::revert_requested;
      bool pointer_dispatched = false;
      if (!expired && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
          event.button.windowID == SDL_GetWindowID(window) &&
          event.button.which != SDL_TOUCH_MOUSEID &&
          event.button.button == SDL_BUTTON_LEFT && event.button.clicks == 1 &&
          (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0) {
        int logical_width{}, logical_height{}, pixel_width{}, pixel_height{};
        if (SDL_GetWindowSize(window, &logical_width, &logical_height) &&
            SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height) &&
            logical_width > 0 && logical_height > 0 && pixel_width > 0 &&
            pixel_height > 0) {
          const ui::UiExtent pixels{static_cast<std::uint32_t>(pixel_width),
                                    static_cast<std::uint32_t>(pixel_height)};
          const auto point = ui::map_graphics_menu_pointer_to_pixels(
              event.button.x, event.button.y,
              {static_cast<std::uint32_t>(logical_width),
               static_cast<std::uint32_t>(logical_height)}, pixels);
          if (point) {
            effect = ui::dispatch_graphics_menu_pointer(
                menu, pixels, (*point)[0], (*point)[1], ui::GraphicsClock::now());
            pointer_dispatched = true;
          }
        }
      }
      if (!expired && translated_key) {
        const auto key = *translated_key;
        if (menu.phase() == ui::GraphicsMenuPhase::confirming &&
            pressed && !repeated &&
            (key == ui::GraphicsMenuKey::enter ||
             key == ui::GraphicsMenuKey::space)) {
          effect = menu.confirm();
        } else {
          effect = menu.handle_key(key, pressed, repeated);
        }
      }
      if (expired || translated_key || pointer_dispatched) {
        if (effect == ui::GraphicsMenuEffect::quit_requested) {
          running = false;
        } else if (effect == ui::GraphicsMenuEffect::apply_requested) {
          if (const auto proposal = menu.request_apply()) {
            const auto transaction = settings::apply_graphics_transaction(
                menu.confirmed_effective(), proposal->effective,
                [&](const settings::EffectiveGraphicsSettings &value) {
                  return apply_graphics(device, window, value);
                });
            if (transaction == settings::GraphicsApplyTransaction::restore_failed) {
              result = failure("graphics configuration recovery failed");
              running = false;
              break;
            }
            const bool applied =
                transaction == settings::GraphicsApplyTransaction::applied;
            const auto acknowledged =
                menu.acknowledge_apply(applied, ui::GraphicsClock::now());
            if (acknowledged == ui::GraphicsMenuEffect::commit_requested) {
              if (!graphics_settings_path.empty())
                static_cast<void>(settings::save_graphics_settings(
                    graphics_settings_path, menu.confirmed_requested()));
            }
          }
        } else if (effect == ui::GraphicsMenuEffect::revert_requested) {
          const auto transaction = settings::apply_graphics_transaction(
              menu.live_effective(), menu.confirmed_effective(),
              [&](const settings::EffectiveGraphicsSettings &value) {
                return apply_graphics(device, window, value);
              });
          if (transaction == settings::GraphicsApplyTransaction::restore_failed) {
            result = failure("graphics configuration recovery failed");
            running = false;
            break;
          }
          const bool restored =
              transaction == settings::GraphicsApplyTransaction::applied;
          static_cast<void>(menu.acknowledge_revert(restored));
        } else if (effect == ui::GraphicsMenuEffect::commit_requested) {
          if (!graphics_settings_path.empty())
            static_cast<void>(settings::save_graphics_settings(
                graphics_settings_path, menu.confirmed_requested()));
        }
      }
    }
    if (!running)
      break;
    if (menu.tick(ui::GraphicsClock::now()) ==
        ui::GraphicsMenuEffect::revert_requested) {
      const auto transaction = settings::apply_graphics_transaction(
          menu.live_effective(), menu.confirmed_effective(),
          [&](const settings::EffectiveGraphicsSettings &value) {
            return apply_graphics(device, window, value);
          });
      if (transaction == settings::GraphicsApplyTransaction::restore_failed) {
        result = failure("graphics configuration recovery failed");
        break;
      }
      const bool restored =
          transaction == settings::GraphicsApplyTransaction::applied;
      static_cast<void>(menu.acknowledge_revert(restored));
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
    SDL_GPUTexture *presentation_target =
        capture_texture != nullptr ? capture_texture : swapchain;
    const auto presentation = resolve_runtime_presentation_settings(
        menu.live_effective(), {swapchain_width, swapchain_height});
    if (!presentation) {
      result = {.success = false,
                .message = "effective graphics presentation is unsupported"};
      SDL_SubmitGPUCommandBuffer(command);
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      break;
    }
    SDL_GPUTexture *content_target = presentation_target;
    if (presentation->spatial_resample) {
      const auto format = SDL_GetGPUSwapchainTextureFormat(device, window);
      if (!ensure_render_scale_target(device, presentation->content_extent.width,
                                      presentation->content_extent.height, format,
                                      render_scale_target)) {
        result = failure("render scale target creation failed");
        SDL_SubmitGPUCommandBuffer(command);
        if (capture_transfer != nullptr)
          SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
        if (capture_texture != nullptr)
          SDL_ReleaseGPUTexture(device, capture_texture);
        break;
      }
      content_target = render_scale_target.texture;
    }
    const Uint32 content_width = presentation->content_extent.width;
    const Uint32 content_height = presentation->content_extent.height;
    // This path is intentionally limited to the explicit source-only
    // diagnostic scene. Normal startup has no recovered world producer and
    // must not allocate fabricated temporal inputs.
    const bool temporal_diagnostic = scene != nullptr &&
                                     presentation->profile == Mode::modern &&
                                     presentation_target != nullptr;
    if (temporal_diagnostic) {
      const auto format = SDL_GetGPUSwapchainTextureFormat(device, window);
      if (!ensure_temporal_resolve_baseline(
              device, window, swapchain_width, swapchain_height, content_width,
              content_height, format, temporal_baseline)) {
        result = failure("portable temporal diagnostic resource creation failed");
        SDL_SubmitGPUCommandBuffer(command);
        if (capture_transfer != nullptr)
          SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
        if (capture_texture != nullptr)
          SDL_ReleaseGPUTexture(device, capture_texture);
        break;
      }
      content_target = temporal_baseline.current.texture;
    }
    if (scene && swapchain != nullptr &&
        !ensure_scene_depth(device, content_width, content_height, gpu)) {
      result = failure("scene depth target creation failed");
      SDL_SubmitGPUCommandBuffer(command);
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      break;
    }
    const auto draw_list = ui::build_graphics_menu_draw_list(
        menu, {swapchain_width, swapchain_height}, ui::GraphicsClock::now(),
        1.0F, explicit_locale, platform_locales);
    const auto overlay_batch =
        draw_list.status == ui::UiBuildStatus::ok
            ? build_overlay_batch(draw_list, ui_fonts, overlay.fonts)
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
    std::optional<IntroPreviewDiagnosticSubmission> diagnostic_submission;
    std::unique_ptr<SdlIntroFrame> diagnostic_intro_frame;
    if (intro_preview_diagnostic != nullptr) {
      try {
        diagnostic_submission.emplace(IntroPreviewDiagnosticSubmission::build(
            *intro_preview_diagnostic, content_width, content_height,
            SDL_GetGPUSwapchainTextureFormat(device, window)));
        diagnostic_intro_frame = gpu_intro->prepare(command,
                                                     diagnostic_submission->draws());
      } catch (const std::exception &error) {
        result = {.success = false,
                  .message = std::string("intro diagnostic frame preparation failed: ") +
                             error.what()};
        SDL_SubmitGPUCommandBuffer(command);
        SDL_WaitForGPUIdle(device);
        release_overlay_transfers();
        break;
      }
    }
    // Modern mode remains spatial-only until a renderer binds and submits a
    // complete temporal resolve pass.  In particular, do not acquire history,
    // allocate vector/history targets, or jitter this scene's projection:
    // doing any of those without a consuming resolve would change the image
    // while still advertising no temporal capability.  The renderer-neutral
    // TemporalHistoryLifecycle, TemporalJitterProvider and
    // TemporalResolveInputLifecycle remain the contract for that future pass.
    SceneInstanceHistorySubmissionGuard scene_history_frame{
        scene_instance_history};
    if (scene != nullptr && swapchain != nullptr) {
      const auto scene_submission =
          graphics::make_scene_gpu_instance_submission(scene->instances);
      std::vector<std::uint64_t> current_identities;
      current_identities.reserve(scene_submission.size());
      for (const auto &instance : scene_submission)
        current_identities.push_back(instance.identity);
      // A different source-plan instance set cannot share prior snapshots.
      if (current_identities != scene_instance_identities) {
        scene_instance_history.invalidate();
        if (!scene_instance_history.initialize(scene_submission)) {
          result = failure("scene instance history reinitialization failed");
          SDL_SubmitGPUCommandBuffer(command);
          SDL_WaitForGPUIdle(device);
          release_overlay_transfers();
          break;
        }
        scene_instance_identities = std::move(current_identities);
      }
      const std::array<Uint32, 2> current_extent{content_width, content_height};
      if (scene_history_extent && *scene_history_extent != current_extent)
        scene_instance_history.invalidate();
      scene_history_extent = current_extent;
      if (!scene_history_frame.begin(scene_submission)) {
        result = failure("scene instance history frame acquisition failed");
        SDL_SubmitGPUCommandBuffer(command);
        SDL_WaitForGPUIdle(device);
        release_overlay_transfers();
        break;
      }
    }
    std::optional<graphics::TemporalResolveInputs> temporal_inputs;
    SDL_GPUTransferBuffer *temporal_vertex_transfer = nullptr;
    if (temporal_diagnostic) {
      const graphics::TemporalResolveResourceSet resources{
          .color = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
              temporal_baseline.current.texture)),
          .depth = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(gpu.depth)),
          .motion_vectors = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
              temporal_baseline.motion.texture)),
          .exposure = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
              temporal_baseline.exposure.texture)),
          .reactive_mask = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
              temporal_baseline.reactive.texture)),
          .hudless_color = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
              temporal_baseline.current.texture)),
          .history = {static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
                          temporal_baseline.history[0].texture)),
                      static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
                          temporal_baseline.history[1].texture))},
          // A clear pass below writes this complete diagnostic producer set
          // before the resolve pass consumes it.
          .motion_vectors_written = true};
      temporal_inputs = temporal_baseline.contract.begin_frame(
          true, {swapchain_width, swapchain_height},
          {content_width, content_height}, resources);
      if (!temporal_inputs) {
        result = {.success = false,
                  .message = "portable temporal diagnostic input admission failed"};
        SDL_SubmitGPUCommandBuffer(command);
        release_overlay_transfers();
        break;
      }
      constexpr std::array<PreviewVertex, 12> temporal_vertices{{
          {{-1, 1, 0}, {1, 1, 1, 0.125F}, {0, 0}},
          {{1, 1, 0}, {1, 1, 1, 0.125F}, {1, 0}},
          {{1, -1, 0}, {1, 1, 1, 0.125F}, {1, 1}},
          {{-1, 1, 0}, {1, 1, 1, 0.125F}, {0, 0}},
          {{1, -1, 0}, {1, 1, 1, 0.125F}, {1, 1}},
          {{-1, -1, 0}, {1, 1, 1, 0.125F}, {0, 1}},
          {{-1, 1, 0}, {1, 1, 1, 0.875F}, {0, 0}},
          {{1, 1, 0}, {1, 1, 1, 0.875F}, {1, 0}},
          {{1, -1, 0}, {1, 1, 1, 0.875F}, {1, 1}},
          {{-1, 1, 0}, {1, 1, 1, 0.875F}, {0, 0}},
          {{1, -1, 0}, {1, 1, 1, 0.875F}, {1, 1}},
          {{-1, -1, 0}, {1, 1, 1, 0.875F}, {0, 1}}}};
      temporal_vertex_transfer = make_upload_transfer(
          device, temporal_vertices.data(), static_cast<Uint32>(sizeof(temporal_vertices)));
      SDL_GPUCopyPass *copy = temporal_vertex_transfer == nullptr
                                  ? nullptr
                                  : SDL_BeginGPUCopyPass(command);
      if (copy == nullptr) {
        if (temporal_vertex_transfer != nullptr)
          SDL_ReleaseGPUTransferBuffer(device, temporal_vertex_transfer);
        temporal_baseline.contract.cancel_submission();
        result = failure("portable temporal diagnostic vertex upload failed");
        SDL_SubmitGPUCommandBuffer(command);
        release_overlay_transfers();
        break;
      }
      const SDL_GPUTransferBufferLocation source{.transfer_buffer = temporal_vertex_transfer};
      const SDL_GPUBufferRegion destination{.buffer = temporal_baseline.vertices,
                                             .size = static_cast<Uint32>(sizeof(temporal_vertices))};
      SDL_UploadToGPUBuffer(copy, &source, &destination, false);
      SDL_EndGPUCopyPass(copy);
    }
    if (swapchain != nullptr) {
      const SDL_GPUColorTargetInfo target{
          .texture = content_target,
          .clear_color = presentation->profile == Mode::original
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
          SDL_BeginGPURenderPass(command, &target, 1, scene ? &depth_target : nullptr);
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
      if (scene)
        for (const auto &draw : scene->draws) {
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
            auto scene_uniform = graphics::make_scene_diagnostic_matrices(
                *scene, draw.instance_index, content_width, content_height);
            if (temporal_inputs) {
              // Matrices are row-major and the bundled vertex shader evaluates
              // row vectors, so clip-space translation occupies row three.
              scene_uniform.projection_view[12] +=
                  temporal_inputs->jitter.internal_ndc_offset[0];
              scene_uniform.projection_view[13] +=
                  temporal_inputs->jitter.internal_ndc_offset[1];
            }
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
      if (diagnostic_intro_frame != nullptr)
        diagnostic_intro_frame->draw(command, pass);
      SDL_EndGPURenderPass(pass);

      if (temporal_inputs) {
        const std::array producer_targets{
            SDL_GPUColorTargetInfo{.texture = temporal_baseline.motion.texture,
                                   .clear_color = {0, 0, 0, 0},
                                   .load_op = SDL_GPU_LOADOP_CLEAR,
                                   .store_op = SDL_GPU_STOREOP_STORE},
            SDL_GPUColorTargetInfo{.texture = temporal_baseline.exposure.texture,
                                   .clear_color = {1, 1, 1, 1},
                                   .load_op = SDL_GPU_LOADOP_CLEAR,
                                   .store_op = SDL_GPU_STOREOP_STORE},
            SDL_GPUColorTargetInfo{.texture = temporal_baseline.reactive.texture,
                                   .clear_color = {0, 0, 0, 0},
                                   .load_op = SDL_GPU_LOADOP_CLEAR,
                                   .store_op = SDL_GPU_STOREOP_STORE}};
        SDL_GPURenderPass *producer_pass = SDL_BeginGPURenderPass(
            command, producer_targets.data(), static_cast<Uint32>(producer_targets.size()), nullptr);
        if (producer_pass == nullptr) {
          temporal_baseline.contract.cancel_submission();
          result = failure("portable temporal diagnostic producer pass creation failed");
          SDL_SubmitGPUCommandBuffer(command);
          release_overlay_transfers();
          break;
        }
        SDL_EndGPURenderPass(producer_pass);
        const auto output_slot = temporal_inputs->history_frame.output_slot;
        const SDL_GPUColorTargetInfo resolve_target{
            .texture = temporal_baseline.history[output_slot].texture,
            .clear_color = {0, 0, 0, 1},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE};
        SDL_GPURenderPass *resolve_pass =
            SDL_BeginGPURenderPass(command, &resolve_target, 1, nullptr);
        if (resolve_pass == nullptr) {
          temporal_baseline.contract.cancel_submission();
          result = failure("portable temporal diagnostic resolve-pass creation failed");
          SDL_SubmitGPUCommandBuffer(command);
          release_overlay_transfers();
          break;
        }
        SDL_BindGPUGraphicsPipeline(resolve_pass, temporal_baseline.pipeline);
        const SDL_GPUBufferBinding temporal_vertices{
            .buffer = temporal_baseline.vertices, .offset = 0};
        SDL_BindGPUVertexBuffers(resolve_pass, 0, &temporal_vertices, 1);
        SDL_PushGPUVertexUniformData(command, 0, matrices.data(), sizeof(matrices));
        if (temporal_inputs->history_frame.history_valid) {
          const SDL_GPUTextureSamplerBinding history_binding{
              .texture = temporal_baseline.history[temporal_inputs->history_frame.history_slot].texture,
              .sampler = temporal_baseline.sampler};
          SDL_BindGPUFragmentSamplers(resolve_pass, 0, &history_binding, 1);
          SDL_DrawGPUPrimitives(resolve_pass, 6, 1, 0, 0);
        }
        const SDL_GPUTextureSamplerBinding current_binding{
            .texture = temporal_baseline.current.texture,
            .sampler = temporal_baseline.sampler};
        SDL_BindGPUFragmentSamplers(resolve_pass, 0, &current_binding, 1);
        SDL_DrawGPUPrimitives(resolve_pass, 6, 1, 6, 0);
        SDL_EndGPURenderPass(resolve_pass);
        const SDL_GPUBlitInfo resolve_blit{
            .source = {.texture = temporal_baseline.history[output_slot].texture,
                       .w = swapchain_width, .h = swapchain_height},
            .destination = {.texture = presentation_target,
                            .w = swapchain_width, .h = swapchain_height},
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .flip_mode = SDL_FLIP_NONE,
            .filter = SDL_GPU_FILTER_LINEAR,
            .cycle = false};
        SDL_BlitGPUTexture(command, &resolve_blit);
      } else if (content_target != presentation_target) {
        const SDL_GPUBlitInfo scale_blit{
            .source = {.texture = content_target,
                       .w = content_width,
                       .h = content_height},
            .destination = {.texture = presentation_target,
                            .w = swapchain_width,
                            .h = swapchain_height},
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .flip_mode = SDL_FLIP_NONE,
            .filter = SDL_GPU_FILTER_LINEAR,
            .cycle = false};
        SDL_BlitGPUTexture(command, &scale_blit);
      }

      if (!overlay_batch.vertices.empty()) {
        const SDL_GPUColorTargetInfo overlay_target{
          .texture = presentation_target,
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
      if (temporal_inputs)
        temporal_baseline.contract.cancel_submission();
      result = failure("SDL GPU command-buffer submission failed");
      if (temporal_vertex_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, temporal_vertex_transfer);
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      release_overlay_transfers();
      break;
    }
    if (temporal_vertex_transfer != nullptr)
      SDL_ReleaseGPUTransferBuffer(device, temporal_vertex_transfer);
    if (temporal_inputs && !temporal_baseline.contract.commit_submission()) {
      result = {.success = false,
                .message = "portable temporal diagnostic submission commit failed"};
      if (capture_transfer != nullptr)
        SDL_ReleaseGPUTransferBuffer(device, capture_transfer);
      if (capture_texture != nullptr)
        SDL_ReleaseGPUTexture(device, capture_texture);
      release_overlay_transfers();
      break;
    }
    if (scene != nullptr && swapchain != nullptr &&
        !scene_history_frame.commit()) {
      result = {.success = false,
                .message = "scene instance history frame commit failed"};
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
          std::error_code remove_error;
          std::filesystem::remove(temporary_path, remove_error);
          if (remove_error) {
            result = {.success = false,
                      .message = "screenshot temporary-file cleanup failed: " +
                                 remove_error.message()};
          }
          const auto utf8_path = temporary_path.u8string();
          if (result.success &&
              (surface == nullptr ||
               !SDL_SaveBMP(surface,
                            reinterpret_cast<const char *>(utf8_path.c_str())))) {
            result = failure("screenshot BMP save failed");
          }
          if (result.success) {
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
          if (!result.success) {
            std::error_code cleanup_error;
            std::filesystem::remove(temporary_path, cleanup_error);
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
    if (diagnostic_intro_frame != nullptr)
      SDL_WaitForGPUIdle(device);
    release_overlay_transfers();
    ++frames;
    if (frame_limit != 0 && frames >= frame_limit)
      running = false;
  }
  SDL_WaitForGPUIdle(device);
  if (result.success && intro)
    result.message += " (" + std::to_string(gpu_intro->image_count()) +
        " source-backed intro images uploaded; automatic intro playback pending)";
  if (result.success && intro_preview_diagnostic != nullptr)
    result.message += " (source-backed intro picture rendered with generic fit projection; playback pending)";
  gpu_intro.reset();
  release_overlay(device, overlay);
  release_startup_images(device, gpu_startup);
  release_temporal_resolve_baseline(device, temporal_baseline);
  release_render_scale_target(device, render_scale_target);
  release_scene(device, gpu);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyGPUDevice(device);
  return result;
}

} // namespace off::platform
