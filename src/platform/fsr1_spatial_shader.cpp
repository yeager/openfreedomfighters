#include "off/platform/fsr1_spatial_shader.hpp"
#include "off/platform/generated/fsr1_spatial_shaders.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace off::platform {
namespace {

struct ShaderSource {
  const Uint8 *code{};
  std::size_t size{};
  const char *entrypoint{};
  SDL_GPUShaderFormat format{};
};

ShaderSource source_for(SDL_GPUShaderFormat formats,
                        Fsr1SpatialShaderStage stage) {
  if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
    switch (stage) {
    case Fsr1SpatialShaderStage::fullscreen_vertex:
      return {generated::fsr1_fullscreen_vert_dxil,
              sizeof(generated::fsr1_fullscreen_vert_dxil), "main",
              SDL_GPU_SHADERFORMAT_DXIL};
    case Fsr1SpatialShaderStage::easu_fragment:
      return {generated::fsr1_easu_dxil, sizeof(generated::fsr1_easu_dxil),
              "main", SDL_GPU_SHADERFORMAT_DXIL};
    case Fsr1SpatialShaderStage::rcas_fragment:
      return {generated::fsr1_rcas_dxil, sizeof(generated::fsr1_rcas_dxil),
              "main", SDL_GPU_SHADERFORMAT_DXIL};
    }
  }
  if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
    switch (stage) {
    case Fsr1SpatialShaderStage::fullscreen_vertex:
      return {reinterpret_cast<const Uint8 *>(generated::fsr1_fullscreen_vert_msl),
              sizeof(generated::fsr1_fullscreen_vert_msl) - 1, "main0",
              SDL_GPU_SHADERFORMAT_MSL};
    case Fsr1SpatialShaderStage::easu_fragment:
      return {reinterpret_cast<const Uint8 *>(generated::fsr1_easu_msl),
              sizeof(generated::fsr1_easu_msl) - 1, "main0",
              SDL_GPU_SHADERFORMAT_MSL};
    case Fsr1SpatialShaderStage::rcas_fragment:
      return {reinterpret_cast<const Uint8 *>(generated::fsr1_rcas_msl),
              sizeof(generated::fsr1_rcas_msl) - 1, "main0",
              SDL_GPU_SHADERFORMAT_MSL};
    }
  }
  if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
    switch (stage) {
    case Fsr1SpatialShaderStage::fullscreen_vertex:
      return {generated::fsr1_fullscreen_vert_spv,
              sizeof(generated::fsr1_fullscreen_vert_spv), "main",
              SDL_GPU_SHADERFORMAT_SPIRV};
    case Fsr1SpatialShaderStage::easu_fragment:
      return {generated::fsr1_easu_spv, sizeof(generated::fsr1_easu_spv),
              "main", SDL_GPU_SHADERFORMAT_SPIRV};
    case Fsr1SpatialShaderStage::rcas_fragment:
      return {generated::fsr1_rcas_spv, sizeof(generated::fsr1_rcas_spv),
              "main", SDL_GPU_SHADERFORMAT_SPIRV};
    }
  }
  throw std::runtime_error("FSR 1 shaders require DXIL, MSL or SPIR-V");
}

} // namespace

SDL_GPUShader *create_fsr1_spatial_shader(SDL_GPUDevice *device,
                                          Fsr1SpatialShaderStage stage) {
  if (device == nullptr)
    throw std::runtime_error("FSR 1 shader creation requires a live SDL GPU device");
  const auto source = source_for(SDL_GetGPUShaderFormats(device), stage);
  const bool fragment = stage != Fsr1SpatialShaderStage::fullscreen_vertex;
  const SDL_GPUShaderCreateInfo info{
      .code_size = source.size,
      .code = source.code,
      .entrypoint = source.entrypoint,
      .format = source.format,
      .stage = fragment ? SDL_GPU_SHADERSTAGE_FRAGMENT
                        : SDL_GPU_SHADERSTAGE_VERTEX,
      .num_samplers = fragment ? 1U : 0U,
      .num_storage_textures = 0,
      .num_storage_buffers = 0,
      .num_uniform_buffers = fragment ? 1U : 0U,
      .props = 0};
  auto *shader = SDL_CreateGPUShader(device, &info);
  if (shader == nullptr)
    throw std::runtime_error(std::string("FSR 1 SDL GPU shader creation failed: ") +
                             SDL_GetError());
  return shader;
}

} // namespace off::platform
