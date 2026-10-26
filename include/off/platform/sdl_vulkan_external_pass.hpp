#pragma once

#if !defined(OFF_HAS_SDL_VULKAN_INTEROP)
#error "SDL Vulkan external-pass support is available only in interop builds"
#endif

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <span>
#include <string>

namespace off {
namespace platform {

struct SdlVulkanExternalPassResult {
  bool success{};
  std::string error;
};

// Records a synchronous Vulkan callback into an SDL-owned command buffer.
// The callback must not retain its context, textures, or command buffer, submit
// work, or change any listed image layout. SDL owns transitions and submission.
// This adapter does not enable an upscaler capability or own Vulkan resources.
// A failed SDL pass must be treated as command-buffer failure; cancel it instead
// of submitting it.
[[nodiscard]] SdlVulkanExternalPassResult record_sdl_vulkan_external_pass(
    SDL_GPUCommandBuffer *command,
    std::span<const SDL_GPUVulkanExternalTextureUse> texture_uses,
    SDL_GPUVulkanExternalPassCallback callback, void *userdata);

} // namespace platform
} // namespace off
