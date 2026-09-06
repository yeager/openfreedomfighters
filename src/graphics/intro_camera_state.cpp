#include "off/graphics/intro_camera_state.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
float finite(float value) {
    if (!std::isfinite(value)) throw std::runtime_error("intro camera arithmetic must be finite");
    return value;
}
float narrow(double value) {
    constexpr double limit = std::numeric_limits<float>::max();
    if (!std::isfinite(value) || value < -limit || value > limit)
        throw std::runtime_error("intro camera value is outside the supported binary32 range");
    const volatile float rounded = static_cast<float>(value);
    return finite(rounded);
}
float multiply(float a, float b) {
    const volatile float rounded = a * b;
    return finite(rounded);
}
float add(float a, float b) {
    const volatile float rounded = a + b;
    return finite(rounded);
}
float divide(float a, float b) {
    if (b == 0) throw std::runtime_error("intro camera divisor is zero");
    const volatile float rounded = a / b;
    return finite(rounded);
}
}

IntroCameraState convert_intro_camera_mode_zero(const data::GmsIntroCameraSource& source) {
    if (source.aspect_mode != 0U || source.renderer_list_selector != 0U ||
        std::fegetround() != FE_TONEAREST)
        throw std::runtime_error("intro camera mode or rounding policy is unsupported");
    auto near = narrow(source.near_distance);
    if (near < 1.0F) near = 1.0F;
    const auto far = narrow(source.far_distance);
    const auto auxiliary = narrow(source.auxiliary_scalar);
    const auto degrees = narrow(source.angle_degrees);
    const auto radians = divide(multiply(degrees, std::bit_cast<float>(0x40490fdbU)), 180.0F);
    for (const auto value : source.auxiliary_floats) static_cast<void>(finite(value));
    for (const auto value : source.viewport) static_cast<void>(finite(value));
    if (source.viewport[2] <= 0 || source.viewport[3] <= 0)
        throw std::runtime_error("intro camera viewport extents must be positive");
    // Retain the constructor-identity composition operations, including zeros.
    const std::array<float,4> viewport{
        add(multiply(0.0F, source.viewport[2]), multiply(1.0F, source.viewport[0])),
        add(multiply(0.0F, source.viewport[3]), multiply(1.0F, source.viewport[1])),
        multiply(1.0F, source.viewport[2]), multiply(1.0F, source.viewport[3])};
    const auto ratio = divide(viewport[3], viewport[2]);
    const volatile float priority = static_cast<float>(std::bit_cast<std::int32_t>(source.priority));
    const auto background = ((source.background_rgb[0] & 255U) << 16U) |
                            ((source.background_rgb[1] & 255U) << 8U) |
                            (source.background_rgb[2] & 255U);
    return {source, near, far, auxiliary, radians, viewport, ratio, priority,
            background, source.final_boolean != 0U};
}

} // namespace off::graphics
