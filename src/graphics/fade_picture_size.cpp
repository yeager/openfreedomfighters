#include "off/graphics/fade_picture_size.hpp"

#include <cfenv>
#include <stdexcept>

namespace off::graphics {

void FadePictureSize::initialize(std::int32_t engine_width, std::int32_t engine_height,
                                 PictureSubmissionCache& cache, const InvalidationHook& hook) {
    if (initializing_ || engine_width <= 0 || engine_height <= 0 ||
        std::fegetround() != FE_TONEAREST)
        throw std::runtime_error("fade picture initialization input or reentrancy is unsupported");
    const std::array<float, 2> next{
        static_cast<float>(engine_width / 16 + 1),
        static_cast<float>(engine_height / 16 + 1)};
    if (next == scale_) return;
    if (!hook) throw std::runtime_error("fade picture invalidation hook is required");
    struct Guard {
        bool& active;
        explicit Guard(bool& value) : active(value) { active = true; }
        ~Guard() { active = false; }
    } guard(initializing_);
    scale_[0] = next[0];
    scale_[1] = next[1];
    cache.invalidate();
    hook();
}

} // namespace off::graphics
