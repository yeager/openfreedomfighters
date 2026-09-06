#include "sdl_gpu_witness.hpp"
#include "off/platform/sdl_picture_clear.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr Uint32 width = 32, height = 24, row_pixels = 64;
constexpr Uint32 download_bytes = row_pixels * height * 4;
constexpr Uint8 initial_stencil = 0x35;
constexpr off::graphics::PictureDeviceViewport a{3, 4, 11, 9, 0, 1};
constexpr off::graphics::PictureDeviceViewport b{8, 8, 17, 10, 0, 1};
int failures = 0;
void require(bool condition, const char* operation) {
  if (!condition) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
bool inside(Uint32 x, Uint32 y, const off::graphics::PictureDeviceViewport& rectangle) {
  return x >= rectangle.x && x < rectangle.x + rectangle.width &&
         y >= rectangle.y && y < rectangle.y + rectangle.height;
}
struct Resources {
  SDL_GPUDevice* device{};
  SDL_GPUTexture *color{}, *depth{};
  SDL_GPUTransferBuffer* download{};
  SDL_GPUCommandBuffer* command{};
  std::unique_ptr<off::platform::SdlPictureClear> clear;
  std::unique_ptr<off::test::SdlGpuWitness> witness;
  ~Resources() {
    if (command) SDL_CancelGPUCommandBuffer(command);
    if (device) {
      SDL_WaitForGPUIdle(device);
      witness.reset(); clear.reset();
      if (download) SDL_ReleaseGPUTransferBuffer(device, download);
      if (depth) SDL_ReleaseGPUTexture(device, depth);
      if (color) SDL_ReleaseGPUTexture(device, color);
      SDL_DestroyGPUDevice(device);
    }
    SDL_Quit();
  }
  void acquire() {
    require(command == nullptr, "previous command must be submitted before acquisition");
    command = SDL_AcquireGPUCommandBuffer(device);
    require(command != nullptr, "acquire depth/stencil witness command");
  }
  std::vector<std::uint8_t> readback() {
    auto* copy = SDL_BeginGPUCopyPass(command);
    require(copy != nullptr, "begin witness color readback");
    const SDL_GPUTextureRegion source{color, 0, 0, 0, 0, 0, width, height, 1};
    const SDL_GPUTextureTransferInfo destination{download, 0, row_pixels, height};
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    auto* submitted = command; command = nullptr;
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);
    require(fence != nullptr, "submit depth/stencil witness command");
    const bool completed = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    require(completed, "wait for witness readback fence");
    const auto* mapped = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, download, false));
    require(mapped != nullptr, "map witness readback");
    std::vector<std::uint8_t> result;
    try { result.assign(mapped, mapped + download_bytes); }
    catch (...) { SDL_UnmapGPUTransferBuffer(device, download); throw; }
    SDL_UnmapGPUTransferBuffer(device, download);
    return result;
  }
};

int run(SDL_GPUTextureFormat depth_format, const char* label) {
  Resources gpu;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cout << "SKIP: SDL video unavailable: " << SDL_GetError() << '\n'; return 77;
  }
  gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL |
                                  SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
  if (!gpu.device) {
    std::cout << "SKIP: offscreen GPU unavailable: " << SDL_GetError() << '\n'; return 77;
  }
  if (!SDL_GPUTextureSupportsFormat(gpu.device, depth_format, SDL_GPU_TEXTURETYPE_2D,
                                    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) ||
      !SDL_GPUTextureSupportsFormat(gpu.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                   SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET)) {
    std::cout << "SKIP: required offscreen formats unavailable for " << label << '\n'; return 77;
  }
  try {
    std::cout << "Depth/stencil witness backend: " << SDL_GetGPUDeviceDriver(gpu.device)
              << "; format: " << label << '\n';
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D; info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = width; info.height = height; info.layer_count_or_depth = 1;
    info.num_levels = 1; info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    gpu.color = SDL_CreateGPUTexture(gpu.device, &info);
    require(gpu.color != nullptr, "create witness color target");
    info.format = depth_format; info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    gpu.depth = SDL_CreateGPUTexture(gpu.device, &info);
    require(gpu.depth != nullptr, "create witness depth/stencil target");
    const SDL_GPUTransferBufferCreateInfo download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, download_bytes, 0};
    gpu.download = SDL_CreateGPUTransferBuffer(gpu.device, &download_info);
    require(gpu.download != nullptr, "create witness color download buffer");
    gpu.clear = std::make_unique<off::platform::SdlPictureClear>(gpu.device);
    gpu.witness = std::make_unique<off::test::SdlGpuWitness>(gpu.device, depth_format);
    const off::platform::SdlPictureClearTarget target{gpu.color, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                   gpu.depth, depth_format, width, height};
    const auto seed = [&] {
      gpu.acquire();
      SDL_GPUColorTargetInfo color{};
      color.texture = gpu.color; color.clear_color = {0, 0, 0, 1};
      color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
      SDL_GPUDepthStencilTargetInfo depth{};
      depth.texture = gpu.depth; depth.clear_depth = 0; depth.clear_stencil = initial_stencil;
      depth.load_op = depth.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
      depth.store_op = depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
      auto* pass = SDL_BeginGPURenderPass(gpu.command, &color, 1, &depth);
      require(pass != nullptr, "seed independent known depth/stencil values");
      SDL_EndGPURenderPass(pass);
    };
    std::size_t probes = 0;
    const auto probe = [&](const char* name, bool depth_test, float value, bool stencil_test,
                           Uint8 reference, auto expected) {
      if (!gpu.command) gpu.acquire();
      gpu.witness->draw(gpu.command, target, depth_test, value, stencil_test, reference);
      const auto pixels = gpu.readback();
      std::size_t mismatches = 0;
      for (Uint32 y = 0; y < height; ++y) for (Uint32 x = 0; x < width; ++x) {
        const auto on = expected(x, y);
        for (Uint32 channel = 0; channel < 4; ++channel) {
          const auto wanted = channel == 3 || on ? 255 : 0;
          if (pixels[(y * row_pixels + x) * 4 + channel] != wanted) ++mismatches;
        }
      }
      ++probes;
      if (mismatches) {
        ++failures;
        std::cerr << "FAIL: " << label << ": " << name << "; mismatched witness channels=" << mismatches << '\n';
      }
    };
    const auto all = [](Uint32, Uint32) { return true; };
    const auto none = [](Uint32, Uint32) { return false; };
    const auto in_a = [](Uint32 x, Uint32 y) { return inside(x, y, a); };
    const auto outside_a = [](Uint32 x, Uint32 y) { return !inside(x, y, a); };

    seed();
    probe("baseline depth zero", true, 0, false, 0, all);
    probe("baseline stencil seed", false, 0, true, initial_stencil, all);
    probe("baseline wrong depth rejected", true, 1, false, 0, none);
    probe("baseline wrong stencil rejected", false, 0, true, 0, none);

    seed();
    gpu.clear->encode(gpu.command, target, a, {false, true, false, 0, 1, 0});
    probe("depth-only inside becomes one", true, 1, false, 0, in_a);
    probe("depth-only outside remains zero", true, 0, false, 0, outside_a);
    probe("depth-only preserves stencil everywhere", false, 0, true, initial_stencil, all);
    probe("repeated depth probe did not mutate values", true, 1, false, 0, in_a);

    seed();
    gpu.clear->encode(gpu.command, target, a, {false, false, true, 0, 1, 0});
    probe("stencil-only inside becomes zero", false, 0, true, 0, in_a);
    probe("stencil-only outside retains full seed byte", false, 0, true, initial_stencil, outside_a);
    probe("stencil-only preserves depth everywhere", true, 0, false, 0, all);
    probe("repeated stencil probe did not mutate values", false, 0, true, 0, in_a);

    seed();
    gpu.clear->encode(gpu.command, target, a, {true, false, false, 0xff123456U, 1, 0});
    probe("color-only preserves depth", true, 0, false, 0, all);
    probe("color-only preserves stencil", false, 0, true, initial_stencil, all);

    seed();
    gpu.clear->encode(gpu.command, target, a, {false, true, true, 0, 1, 0});
    probe("combined clear inside passes both tests", true, 1, true, 0, in_a);
    probe("combined clear outside retains both seeds", true, 0, true, initial_stencil, outside_a);

    seed();
    gpu.clear->encode(gpu.command, target, a, {false, true, true, 0, 1, 0});
    gpu.clear->encode(gpu.command, target, b, {false, false, true, 0, 1, 255});
    probe("overlap zero stencil remains only A minus B", false, 0, true, 0,
          [](Uint32 x, Uint32 y) { return inside(x, y, a) && !inside(x, y, b); });
    probe("overlap second stencil writes full byte in B", false, 0, true, 255,
          [](Uint32 x, Uint32 y) { return inside(x, y, b); });
    probe("overlap stencil-only operation preserves depth in A", true, 1, false, 0, in_a);
    probe("overlap stencil-only operation preserves zero depth outside A", true, 0, false, 0, outside_a);
    probe("overlap untouched stencil outside both rectangles", false, 0, true, initial_stencil,
          [](Uint32 x, Uint32 y) { return !inside(x, y, a) && !inside(x, y, b); });

    // Join the real CPU per-view guard to the real GPU encoder. Fog hooks are
    // intentional no-ops here, not a complete picture-rendering pipeline.
    seed();
    off::graphics::PictureViewTransition view;
    off::graphics::IntroCameraState camera{};
    camera.far_distance = 16; camera.viewport = {0, 0, 1, 1};
    off::graphics::PictureViewFogState fog{{0, 0xff000000U, 0xffffffffU, 0}, false, true, 0, 0};
    off::graphics::PictureDeviceViewport active_viewport{};
    std::uint32_t activity = 0; unsigned clears = 0;
    const off::graphics::PictureViewTransitionHooks view_hooks{
      [&](const auto& viewport) { active_viewport = viewport; }, [](bool) {}, [](std::uint32_t) {},
      [] {}, [] {}, [](float) {}, [](float) {},
      [&](const auto& request) { ++clears; gpu.clear->encode(gpu.command, target, active_viewport, request); }};
    const auto transition = [&](std::uint32_t frame, std::uint32_t flags, const auto& r) {
      view.run(frame, camera, flags, 0, {r.x, r.y, r.x + r.width, r.y + r.height},
               1, true, activity, fog, view_hooks);
    };
    transition(17, 0, a);
    transition(17, 0, b);
    probe("same-frame second viewport does not clear depth again", true, 1, false, 0, in_a);
    probe("same-frame second viewport does not clear stencil again", false, 0, true, 0, in_a);
    gpu.acquire();
    transition(18, 0x8000U, b);
    transition(18, 0, b);
    probe("suppressed guard remains consumed after flag removal", true, 1, false, 0, in_a);
    gpu.acquire(); transition(19, 0, b);
    const auto either = [](Uint32 x, Uint32 y) { return inside(x, y, a) || inside(x, y, b); };
    probe("next frame clears new viewport depth while preserving first", true, 1, false, 0, either);
    probe("next frame clears new viewport stencil while preserving first", false, 0, true, 0, either);
    require(clears == 2 && activity == 19 && view.last_clear_frame() == 19,
            "CPU view guard must govern actual GPU clear calls");
    require(SDL_WaitForGPUIdle(gpu.device), "complete witness GPU work before teardown");
    std::cout << "Completed " << probes << " color-readback depth/stencil probes for " << label << '\n';
    return failures == 0 ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Depth/stencil witness failed: " << error.what() << '\n'; return 1;
  }
}
}

int main() {
  unsigned completed = 0;
  for (const auto& [format, name] : std::array{
      std::pair{SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, "D24_UNORM_S8_UINT"},
      std::pair{SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT, "D32_FLOAT_S8_UINT"}}) {
    const auto result = run(format, name);
    if (result == 1) return 1;
    if (result == 0) ++completed;
  }
  if (completed == 0) {
    std::cout << "SKIP: no supported combined depth/stencil format was exercised\n"; return 77;
  }
  return failures == 0 ? 0 : 1;
}
