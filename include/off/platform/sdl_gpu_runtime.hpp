#pragma once

#include "off/mode.hpp"

#include <cstddef>
#include <string>

namespace off::platform {

struct RuntimeResult {
    bool success{false};
    std::string message;
};

[[nodiscard]] RuntimeResult run_sdl_gpu_runtime(
    Mode mode,
    std::size_t frame_limit = 0
);

}  // namespace off::platform
