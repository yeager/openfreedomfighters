#pragma once

#include "off/data/gms_image.hpp"
#include <functional>

namespace off::graphics {

// Explicit runtime flags, never inferred from authored camera options. This
// models enable/disable operations, not construction, copying or registration.
class CameraEnabledState final {
public:
    explicit CameraEnabledState(std::uint32_t runtime_flags) : flags_(runtime_flags) {}
    CameraEnabledState(const CameraEnabledState&) = delete;
    CameraEnabledState& operator=(const CameraEnabledState&) = delete;
    CameraEnabledState(CameraEnabledState&&) = delete;
    CameraEnabledState& operator=(CameraEnabledState&&) = delete;
    [[nodiscard]] std::uint32_t flags() const noexcept { return flags_; }
    [[nodiscard]] bool enabled() const noexcept { return (flags_ & 0x20U) != 0U; }

    // Changed states notify a present renderer before committing the bit.
    // Equal states need no hook. A throwing hook leaves flags unchanged.
    // Stable ownership and no reentry are explicit native safety constraints.
    void set_enabled(bool requested, bool renderer_present,
                     const std::function<void()>& state_change);
    // View preparation clears only 0x2, without an enabled-state notification.
    // This is not full frustum preparation. Reentry from set_enabled rejects.
    void clear_picture_preparation_bit();
private:
    friend class FreshIntroCamera;
    std::uint32_t flags_;
    bool changing_{false};
};

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
    // Current fractions copied from the full source schema, not distances.
    float fog_start_fraction;
    float fog_end_fraction;
};

// Newly constructed camera, aspect mode zero and renderer-list selector zero.
// No camera registration, runtime flag changes or renderer defaults. Requires
// nearest-even rounding, finite representable arithmetic and positive extents
// as explicit native policies. Projection's separate near clamp remains later.
[[nodiscard]] IntroCameraState
convert_intro_camera_mode_zero(const data::GmsIntroCameraSource& source);

} // namespace off::graphics
