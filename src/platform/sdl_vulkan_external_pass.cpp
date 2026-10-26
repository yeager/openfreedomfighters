#include "off/platform/sdl_vulkan_external_pass.hpp"

#include <limits>

namespace off {
namespace platform {
namespace {

[[nodiscard]] SdlVulkanExternalPassResult failure(const char *message) {
  SdlVulkanExternalPassResult result;
  result.error = message;
  return result;
}

[[nodiscard]] bool valid_access(SDL_GPUVulkanExternalTextureAccess access) {
  switch (access) {
  case SDL_GPU_VULKAN_EXTERNAL_TEXTURE_SAMPLED:
  case SDL_GPU_VULKAN_EXTERNAL_TEXTURE_STORAGE_READ:
  case SDL_GPU_VULKAN_EXTERNAL_TEXTURE_STORAGE_WRITE:
  case SDL_GPU_VULKAN_EXTERNAL_TEXTURE_STORAGE_READ_WRITE:
    return true;
  }
  return false;
}

} // namespace

SdlVulkanExternalPassResult record_sdl_vulkan_external_pass(
    SDL_GPUCommandBuffer *command,
    std::span<const SDL_GPUVulkanExternalTextureUse> texture_uses,
    SDL_GPUVulkanExternalPassCallback callback, void *userdata) {
  if (command == nullptr)
    return failure("SDL Vulkan external pass requires a command buffer");
  if (texture_uses.empty() ||
      texture_uses.size() > std::numeric_limits<Uint32>::max()) {
    return failure("SDL Vulkan external pass requires a bounded texture list");
  }
  if (callback == nullptr)
    return failure("SDL Vulkan external pass requires a callback");

  for (std::size_t index = 0; index < texture_uses.size(); ++index) {
    const auto &use = texture_uses[index];
    if (use.texture == nullptr || !valid_access(use.access))
      return failure("SDL Vulkan external pass texture use is invalid");
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (texture_uses[previous].texture == use.texture)
        return failure(
            "SDL Vulkan external pass texture is listed more than once");
    }
  }

  if (!SDL_RunGPUVulkanExternalPass(
          command, texture_uses.data(),
          static_cast<Uint32>(texture_uses.size()), callback, userdata)) {
    const char *const detail = SDL_GetError();
    return failure(detail != nullptr && detail[0] != '\0'
                       ? detail
                       : "SDL Vulkan external pass failed");
  }
  SdlVulkanExternalPassResult result;
  result.success = true;
  return result;
}

} // namespace platform
} // namespace off
