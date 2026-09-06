#include "off/graphics/center_picture_position.hpp"

#include <cfenv>
#include <cmath>
#include <stdexcept>

namespace off::graphics {

void CenterPicturePosition::initialize(std::array<float, 3>& position,
                                      std::uint32_t& runtime_flags,
                                      std::uint32_t& component_status,
                                      std::int32_t engine_width,
                                      std::int32_t engine_height,
                                      PictureSubmissionCache& cache,
                                      const std::function<void()>& update_service) {
    if (initializing_ || &runtime_flags == &component_status ||
        engine_width <= 0 || engine_height <= 0 || std::fegetround() != FE_TONEAREST)
        throw std::runtime_error("picture centering input or reentrancy is unsupported");
    for (const float coordinate : position)
        if (!std::isfinite(coordinate))
            throw std::runtime_error("picture centering requires finite current position");
    const std::array<float, 3> next{static_cast<float>(engine_width / 2),
                                    static_cast<float>(engine_height / 2), 0.0F};
    if (position == next) {
        component_status |= 1U;
        return;
    }
    if (!update_service) throw std::runtime_error("picture position update service is required");
    struct Guard {
        bool& active;
        explicit Guard(bool& value) : active(value) { active = true; }
        ~Guard() { active = false; }
    } guard(initializing_);
    position[0] = next[0];
    position[1] = next[1];
    position[2] = next[2];
    runtime_flags |= 0x100000U;
    update_service();
    cache.invalidate();
    component_status |= 1U;
}

} // namespace off::graphics
