#pragma once

#include <SDL3/SDL.h>

namespace off::platform {

enum class Fsr1SpatialShaderStage {
  fullscreen_vertex,
  easu_fragment,
  rcas_fragment,
};

// Selects one representation that the active SDL GPU backend advertises.
// The generated variants share a single descriptor and uniform layout.
[[nodiscard]] SDL_GPUShader *create_fsr1_spatial_shader(
    SDL_GPUDevice *device, Fsr1SpatialShaderStage stage);

} // namespace off::platform
