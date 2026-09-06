#pragma once

#include "off/graphics/picture_draw_reset.hpp"
#include <SDL3/SDL.h>

namespace off::platform {

// std140 fragment uniform block, slot zero. This is only stage-zero evaluation;
// it does not choose fog, alpha testing, later stages or raster/blend state.
struct alignas(16) PictureStageShaderUniforms {
  std::array<std::uint32_t, 4> rgb;
  std::array<std::uint32_t, 4> alpha;
  std::array<float, 4> texture_factor;
};

// Inputs are resolved live state, not optional material requests. RGB DISABLE
// returns diffuse; alpha DISABLE with active RGB rejects because the public
// fixed-function API leaves that combination undefined. No guessed alpha rule.
[[nodiscard]] PictureStageShaderUniforms pack_picture_stage_uniforms(
    const graphics::PictureTrackedStage& stage, std::uint32_t packed_texture_factor);

// Caller owns the result and keeps the device alive until release. One sampler
// at slot zero and one fragment uniform buffer at slot zero are required.
// Inputs match the pinned SDL_ttf vertex shader (color, UV). DXIL, SPIR-V and MSL are
// packaged; unsupported formats fail explicitly, without a text-shader fallback.
[[nodiscard]] SDL_GPUShader* create_picture_stage_fragment_shader(SDL_GPUDevice* device);

} // namespace off::platform
