#include "off/platform/picture_stage_shader.hpp"
#include "off/platform/generated/picture_stage_frag.hpp"
#include "off/platform/generated/picture_stage_dxil.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace off::platform {
namespace {
std::uint32_t operation(graphics::PictureStageOperation op) {
  using enum graphics::PictureStageOperation;
  switch (op) {
    case select_argument_1: return 0;
    case modulate_twice: return 1;
    case add: return 2;
    case disable: return 3;
  }
  throw std::runtime_error("unsupported picture stage operation");
}
std::uint32_t argument(graphics::PictureStageArgument arg) {
  using enum graphics::PictureStageArgument;
  switch (arg) {
    case texture: return 0;
    case diffuse: return 1;
    case current: return 2;
    case texture_factor: return 3;
  }
  throw std::runtime_error("unsupported picture stage argument");
}
}

PictureStageShaderUniforms pack_picture_stage_uniforms(
    const graphics::PictureTrackedStage& stage, std::uint32_t factor) {
  if (stage.rgb_operation != graphics::PictureStageOperation::disable &&
      stage.alpha_operation == graphics::PictureStageOperation::disable)
    throw std::runtime_error("active RGB with disabled alpha has no defined picture shader contract");
  return {{operation(stage.rgb_operation), argument(stage.rgb_argument_1), argument(stage.rgb_argument_2), 0},
          {operation(stage.alpha_operation), argument(stage.alpha_argument_1), argument(stage.alpha_argument_2), 0},
          {static_cast<float>((factor >> 16U) & 255U) / 255.0F,
           static_cast<float>((factor >> 8U) & 255U) / 255.0F,
           static_cast<float>(factor & 255U) / 255.0F,
           static_cast<float>((factor >> 24U) & 255U) / 255.0F}};
}

std::optional<PictureStageShaderSource>
select_picture_stage_fragment_shader(SDL_GPUShaderFormat formats) {
  if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
    return PictureStageShaderSource{generated::picture_stage_dxil,
                                    sizeof(generated::picture_stage_dxil),
                                    "main", SDL_GPU_SHADERFORMAT_DXIL};
  }
  // SDL's native Metal backend consumes MSL. Prefer it over SPIR-V whenever
  // both are reported so this stage stays aligned with the bundled vertex
  // shader and does not rely on an optional translation layer.
  if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
    return PictureStageShaderSource{
        reinterpret_cast<const Uint8*>(generated::picture_stage_msl),
        sizeof(generated::picture_stage_msl) - 1, "main0",
        SDL_GPU_SHADERFORMAT_MSL};
  }
  if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
    return PictureStageShaderSource{generated::picture_stage_spirv,
                                    sizeof(generated::picture_stage_spirv),
                                    "main", SDL_GPU_SHADERFORMAT_SPIRV};
  }
  return std::nullopt;
}

SDL_GPUShader* create_picture_stage_fragment_shader(SDL_GPUDevice* device) {
  if (!device) throw std::runtime_error("picture stage shader requires a live GPU device");
  SDL_GPUShaderCreateInfo info{};
  const auto source = select_picture_stage_fragment_shader(
      SDL_GetGPUShaderFormats(device));
  if (!source.has_value()) {
    throw std::runtime_error("picture stage shader requires DXIL, SPIR-V or MSL");
  }
  info.code = source->code;
  info.code_size = source->code_size;
  info.format = source->format;
  info.entrypoint = source->entrypoint;
  info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.num_samplers = 1;
  info.num_uniform_buffers = 1;
  auto* shader = SDL_CreateGPUShader(device, &info);
  if (!shader) throw std::runtime_error(std::string("picture stage shader creation failed: ") + SDL_GetError());
  return shader;
}

static_assert(sizeof(PictureStageShaderUniforms) == 48);
static_assert(offsetof(PictureStageShaderUniforms, alpha) == 16);
static_assert(offsetof(PictureStageShaderUniforms, texture_factor) == 32);
} // namespace off::platform
