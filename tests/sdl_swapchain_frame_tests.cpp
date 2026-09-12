#include "off/platform/sdl_swapchain_frame.hpp"

#include <cstdint>
#include <iostream>

int main() {
  using off::platform::SdlSwapchainFrameStatus;
  using off::platform::classify_sdl_swapchain_frame;

  const auto *image = reinterpret_cast<const void *>(std::uintptr_t{1});
  if (classify_sdl_swapchain_frame(nullptr, 0, 0) !=
      SdlSwapchainFrameStatus::temporarily_unavailable) {
    std::cerr << "null image must be transiently unavailable\n";
    return 1;
  }
  if (classify_sdl_swapchain_frame(image, 0, 720) !=
          SdlSwapchainFrameStatus::invalid_extent ||
      classify_sdl_swapchain_frame(image, 1280, 0) !=
          SdlSwapchainFrameStatus::invalid_extent) {
    std::cerr << "drawable image requires a positive extent\n";
    return 1;
  }
  if (classify_sdl_swapchain_frame(image, 1280, 720) !=
      SdlSwapchainFrameStatus::drawable) {
    std::cerr << "positive drawable image was rejected\n";
    return 1;
  }
  return 0;
}
