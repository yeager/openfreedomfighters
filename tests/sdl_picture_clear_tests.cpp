#include "off/platform/sdl_picture_clear.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
constexpr Uint32 width = 32, height = 24, row_pixels = 64;
constexpr Uint32 download_bytes = row_pixels * height * 4;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
struct Resources {
  SDL_GPUDevice* device{};
  SDL_GPUTexture *color{}, *depth{};
  SDL_GPUTransferBuffer* download{};
  SDL_GPUCommandBuffer* command{};
  std::unique_ptr<off::platform::SdlPictureClear> clear;
  ~Resources() {
    if (command) SDL_CancelGPUCommandBuffer(command);
    if (device) {
      SDL_WaitForGPUIdle(device);
      clear.reset();
      if (download) SDL_ReleaseGPUTransferBuffer(device, download);
      if (depth) SDL_ReleaseGPUTexture(device, depth);
      if (color) SDL_ReleaseGPUTexture(device, color);
      SDL_DestroyGPUDevice(device);
    }
    SDL_Quit();
  }
  void acquire() {
    command = SDL_AcquireGPUCommandBuffer(device);
    require(command != nullptr, "acquire offscreen command buffer");
  }
  std::vector<std::uint8_t> readback() {
    auto* copy = SDL_BeginGPUCopyPass(command);
    require(copy != nullptr, "begin color download");
    const SDL_GPUTextureRegion source{color, 0, 0, 0, 0, 0, width, height, 1};
    const SDL_GPUTextureTransferInfo destination{download, 0, row_pixels, height};
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    auto* submitted = command; command = nullptr;
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);
    require(fence != nullptr, "submit offscreen color download");
    const bool finished = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    require(finished, "wait for offscreen download fence");
    const auto* mapped = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, download, false));
    require(mapped != nullptr, "map completed color download");
    std::vector<std::uint8_t> pixels(mapped, mapped + download_bytes);
    SDL_UnmapGPUTransferBuffer(device, download);
    return pixels;
  }
};
using Color = std::array<std::uint8_t, 4>;
constexpr Color seed{17, 34, 51, 68}, first_color{210, 31, 75, 0}, second_color{42, 181, 93, 207};
constexpr off::graphics::PictureDeviceViewport first{3, 4, 11, 9, 0, 1};
constexpr off::graphics::PictureDeviceViewport second{8, 8, 17, 10, 0, 1};
bool inside(Uint32 x, Uint32 y, const off::graphics::PictureDeviceViewport& r) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
std::uint32_t packed(Color value) {
  return (std::uint32_t(value[3]) << 24U) | (std::uint32_t(value[0]) << 16U) |
         (std::uint32_t(value[1]) << 8U) | value[2];
}
void verify(const std::vector<std::uint8_t>& pixels, bool has_first, bool has_second, SDL_GPUTextureFormat format) {
  std::size_t mismatches = 0;
  for (Uint32 y = 0; y < height; ++y) for (Uint32 x = 0; x < width; ++x) {
    const auto wanted = has_second && inside(x, y, second) ? second_color :
                        has_first && inside(x, y, first) ? first_color : seed;
    for (std::size_t channel = 0; channel < 4; ++channel) {
      const auto actual = pixels[(y * row_pixels + x) * 4 + channel];
      const auto source_channel = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM && channel < 3 ? 2 - channel : channel;
      if (actual != wanted[source_channel]) ++mismatches;
    }
  }
  if (mismatches) std::cerr << "Mismatched offscreen channel values: " << mismatches << '\n';
  check(mismatches == 0, "viewport clear preserves every outside pixel and replaces all interior RGBA values exactly");
}
}

int run(SDL_GPUTextureFormat format) {
  Resources gpu;
  // A dummy video backend cannot load Vulkan. Let SDL choose a real/offscreen
  // backend; callers may explicitly select SDL_VIDEODRIVER=offscreen.
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cout << "SKIP: headless SDL video unavailable: " << SDL_GetError() << '\n'; return 77;
  }
  gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL |
                                  SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
  if (!gpu.device) {
    std::cout << "SKIP: offscreen GPU unavailable: " << SDL_GetError() << '\n'; return 77;
  }
  std::cout << "GPU clear backend: " << SDL_GetGPUDeviceDriver(gpu.device) << '\n';
  if (!SDL_GPUTextureSupportsFormat(gpu.device, format,
          SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET)) {
    std::cout << "SKIP: requested offscreen color format unavailable\n"; return 77;
  }
  try {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D; info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = width; info.height = height; info.layer_count_or_depth = 1;
    info.num_levels = 1; info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    gpu.color = SDL_CreateGPUTexture(gpu.device, &info);
    require(gpu.color != nullptr, "create offscreen color target");
    const bool has_depth = SDL_GPUTextureSupportsFormat(gpu.device, SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    if (has_depth) {
      info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM; info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
      gpu.depth = SDL_CreateGPUTexture(gpu.device, &info);
      require(gpu.depth != nullptr, "create offscreen depth target");
    }
    const SDL_GPUTransferBufferCreateInfo download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, download_bytes, 0};
    gpu.download = SDL_CreateGPUTransferBuffer(gpu.device, &download_info);
    require(gpu.download != nullptr, "create color readback buffer");
    gpu.clear = std::make_unique<off::platform::SdlPictureClear>(gpu.device);
    const off::platform::SdlPictureClearTarget target{gpu.color, format,
        gpu.depth, gpu.depth ? SDL_GPU_TEXTUREFORMAT_D16_UNORM : SDL_GPU_TEXTUREFORMAT_INVALID, width, height};
    const off::graphics::PictureViewClear color_first{true, false, false, packed(first_color), 1, 0};
    const off::graphics::PictureViewClear color_second{true, false, false, packed(second_color), 1, 0};
    const auto seed_target = [&] {
      SDL_GPUColorTargetInfo color{};
      color.texture = gpu.color;
      color.clear_color = {seed[0] / 255.0F, seed[1] / 255.0F, seed[2] / 255.0F, seed[3] / 255.0F};
      color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
      SDL_GPUDepthStencilTargetInfo depth{};
      depth.texture = gpu.depth; depth.clear_depth = 0.25F;
      depth.load_op = SDL_GPU_LOADOP_CLEAR; depth.store_op = SDL_GPU_STOREOP_STORE;
      depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
      auto* pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, gpu.depth ? &depth : nullptr);
      require(pass != nullptr, "seed offscreen attachments");
      SDL_EndGPURenderPass(pass);
    };
    gpu.acquire(); seed_target();
    gpu.clear->encode(gpu.command, target, first, color_first);
    verify(gpu.readback(), true, false, format);

    // Multiple uploads/draws in one command buffer exercise resource cycling:
    // the second clear must not overwrite the first clear's vertex payload.
    gpu.acquire(); seed_target();
    gpu.clear->encode(gpu.command, target, first, color_first);
    gpu.clear->encode(gpu.command, target, second, color_second);
    verify(gpu.readback(), true, true, format);
    if (gpu.depth) {
      gpu.acquire();
      const off::graphics::PictureViewClear depth_only{false, true, false, 0xffffffffU, 1, 0};
      gpu.clear->encode(gpu.command, target, {0, 0, width, height, 0, 1}, depth_only);
      verify(gpu.readback(), true, true, format);
      std::cout << "Depth-only clear preserved color; depth contents were not downloaded or verified.\n";
    } else std::cout << "SKIP: depth-only subcase requires D16 target support\n";

    gpu.acquire();
    for (const auto bad : std::array<off::graphics::PictureDeviceViewport, 6>{{
        {0, 0, 0, 1, 0, 1}, {0, 0, 1, 0, 0, 1}, {31, 0, 2, 1, 0, 1},
        {0, 23, 1, 2, 0, 1}, {UINT32_MAX, 0, 1, 1, 0, 1}, {0, 0, 1, 1, 0.5F, 1}}})
      rejects([&] { gpu.clear->encode(gpu.command, target, bad, color_first); }, "invalid clear rectangle rejects before GPU commands");
    auto bad_clear = color_first; bad_clear.depth_value = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { gpu.clear->encode(gpu.command, target, first, bad_clear); }, "nonfinite clear depth rejects");
    auto no_depth = target; no_depth.depth_stencil = nullptr;
    bad_clear = color_first; bad_clear.depth = true;
    rejects([&] { gpu.clear->encode(gpu.command, no_depth, first, bad_clear); }, "depth clear without attachment rejects");
    bad_clear = color_first; bad_clear.stencil = true;
    rejects([&] { gpu.clear->encode(gpu.command, target, first, bad_clear); }, "stencil request without stencil format rejects");
    verify(gpu.readback(), true, true, format);
    require(SDL_WaitForGPUIdle(gpu.device), "complete offscreen GPU test before releasing resources");
    return failures == 0 ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "GPU clear test failed: " << error.what() << '\n'; return 1;
  }
}

int main() {
  const int rgba = run(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
  if (rgba != 0) return rgba;
  const int bgra = run(SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
  if (bgra == 77) { std::cout << "SKIP: BGRA-only subcase unavailable; RGBA checks passed\n"; return 0; }
  return bgra;
}
