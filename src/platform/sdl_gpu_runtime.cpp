#include "off/platform/sdl_gpu_runtime.hpp"

#include <SDL3/SDL.h>

#include <string>

namespace off::platform {
namespace {

[[nodiscard]] RuntimeResult failure(const char* operation) {
    return {
        .success = false,
        .message = std::string(operation) + ": " + SDL_GetError(),
    };
}

}  // namespace

RuntimeResult run_sdl_gpu_runtime(Mode mode, std::size_t frame_limit) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return failure("SDL initialization failed");
    }

    SDL_Window* window = SDL_CreateWindow(
        "OpenFreedomFighters",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (window == nullptr) {
        const auto result = failure("SDL window creation failed");
        SDL_Quit();
        return result;
    }

    constexpr SDL_GPUShaderFormat shader_formats =
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
        SDL_GPU_SHADERFORMAT_METALLIB;
#ifndef NDEBUG
    constexpr bool debug_device = true;
#else
    constexpr bool debug_device = false;
#endif
    SDL_GPUDevice* device =
        SDL_CreateGPUDevice(shader_formats, debug_device, nullptr);
    if (device == nullptr) {
        const auto result = failure("SDL GPU-device creation failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        const auto result = failure("SDL GPU swapchain creation failed");
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }

    bool running = true;
    std::size_t rendered_frames = 0;
    RuntimeResult result{
        .success = true,
        .message = std::string("Renderer: SDL GPU/") +
                   SDL_GetGPUDeviceDriver(device),
    };
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            result = failure("SDL GPU command-buffer acquisition failed");
            break;
        }
        SDL_GPUTexture* swapchain = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                command, window, &swapchain, nullptr, nullptr)) {
            result = failure("SDL GPU swapchain acquisition failed");
            SDL_CancelGPUCommandBuffer(command);
            break;
        }
        if (swapchain != nullptr) {
            const auto clear_color = mode == Mode::original
                                         ? SDL_FColor{0.0F, 0.0F, 0.0F, 1.0F}
                                         : SDL_FColor{0.015F, 0.025F, 0.05F, 1.0F};
            const SDL_GPUColorTargetInfo target{
                .texture = swapchain,
                .clear_color = clear_color,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
            };
            SDL_GPURenderPass* pass =
                SDL_BeginGPURenderPass(command, &target, 1, nullptr);
            if (pass == nullptr) {
                result = failure("SDL GPU render-pass creation failed");
                SDL_SubmitGPUCommandBuffer(command);
                break;
            }
            SDL_EndGPURenderPass(pass);
        }
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            result = failure("SDL GPU command-buffer submission failed");
            break;
        }
        ++rendered_frames;
        if (frame_limit != 0 && rendered_frames >= frame_limit) {
            running = false;
        }
    }

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}

}  // namespace off::platform
