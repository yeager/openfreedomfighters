#pragma once

#include <cstdint>

namespace off::platform {

// SDL can successfully acquire a command buffer while no drawable swapchain
// image exists (for example while the window is minimized). That is a normal
// transient condition, distinct from a failed acquire or an invalid image.
enum class SdlSwapchainFrameStatus {
  drawable,
  temporarily_unavailable,
  invalid_extent,
};

[[nodiscard]] constexpr SdlSwapchainFrameStatus classify_sdl_swapchain_frame(
    const void *texture, std::uint32_t width, std::uint32_t height) noexcept {
  if (texture == nullptr)
    return SdlSwapchainFrameStatus::temporarily_unavailable;
  if (width == 0 || height == 0)
    return SdlSwapchainFrameStatus::invalid_extent;
  return SdlSwapchainFrameStatus::drawable;
}

} // namespace off::platform
