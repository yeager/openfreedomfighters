#pragma once

#include "off/data/gms_image.hpp"

namespace off::graphics {

struct IntroCameraState {
    // Retain opaque options and original precision separately from conversions.
    data::GmsIntroCameraSource authored;
    float near_distance;
    float far_distance;
    float auxiliary_scalar;
    float angle_radians;
    std::array<float, 4> viewport;
    float viewport_ratio;
    float registration_priority;
    std::uint32_t background;
    bool final_boolean;
};

// Newly constructed camera, aspect mode zero and renderer-list selector zero.
// No camera registration, runtime flag changes or renderer defaults. Requires
// nearest-even rounding, finite representable arithmetic and positive extents
// as explicit native policies. Projection's separate near clamp remains later.
[[nodiscard]] IntroCameraState
convert_intro_camera_mode_zero(const data::GmsIntroCameraSource& source);

} // namespace off::graphics
