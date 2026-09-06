#pragma once

#include "off/graphics/picture_view_transition.hpp"
#include <SDL3/SDL.h>
#include <memory>

namespace off::platform {
struct SdlPictureClearTarget {
  SDL_GPUTexture* color;
  SDL_GPUTextureFormat color_format;
  SDL_GPUTexture* depth_stencil;
  SDL_GPUTextureFormat depth_stencil_format;
  Uint32 width, height;
};

// Executes a viewport-bounded clear using a dedicated draw pass with LOAD/STORE
// attachments. Never clears the full target as an incidental load operation.
// Device, target metadata and command buffer must be live and consistent.
// No pass may be active on the supplied command buffer. Single-sample, mip-zero,
// layer-zero targets only. Does not submit the command buffer or admit a view.
// Color targets are linear RGBA8/BGRA8 UNORM: sRGB/HDR need separate encoding.
// Integer viewport components must be exactly representable through 2^24.
class SdlPictureClear final {
public:
  explicit SdlPictureClear(SDL_GPUDevice* device);
  ~SdlPictureClear();
  SdlPictureClear(const SdlPictureClear&) = delete;
  SdlPictureClear& operator=(const SdlPictureClear&) = delete;
  void encode(SDL_GPUCommandBuffer* command, const SdlPictureClearTarget& target,
              const graphics::PictureDeviceViewport& viewport,
              const graphics::PictureViewClear& clear);
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace off::platform
