#include "off/platform/picture_stage_shader.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <string_view>

namespace {
int failures = 0;

void check(bool value, const char* message) {
  if (!value) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void check_selection(SDL_GPUShaderFormat available,
                     SDL_GPUShaderFormat expected_format,
                     std::string_view expected_entrypoint,
                     const char* message) {
  const auto source =
      off::platform::select_picture_stage_fragment_shader(available);
  check(source.has_value() && source->format == expected_format &&
            source->code != nullptr && source->code_size != 0 &&
            std::string_view(source->entrypoint) == expected_entrypoint,
        message);
}
} // namespace

int main() {
  check(!off::platform::select_picture_stage_fragment_shader(0).has_value(),
        "empty device capability set is rejected");
  check_selection(SDL_GPU_SHADERFORMAT_SPIRV, SDL_GPU_SHADERFORMAT_SPIRV,
                  "main", "SPIR-V source is selected when it is the only format");
  check_selection(SDL_GPU_SHADERFORMAT_MSL, SDL_GPU_SHADERFORMAT_MSL, "main0",
                  "MSL source is selected when it is the only format");
  check_selection(SDL_GPU_SHADERFORMAT_DXIL, SDL_GPU_SHADERFORMAT_DXIL, "main",
                  "DXIL source is selected when it is the only format");
  check_selection(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV,
                  SDL_GPU_SHADERFORMAT_MSL, "main0",
                  "native MSL is preferred over portable SPIR-V");
  check_selection(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL |
                      SDL_GPU_SHADERFORMAT_SPIRV,
                  SDL_GPU_SHADERFORMAT_DXIL, "main",
                  "DXIL remains the Windows-native first choice");
  return failures == 0 ? 0 : 1;
}
