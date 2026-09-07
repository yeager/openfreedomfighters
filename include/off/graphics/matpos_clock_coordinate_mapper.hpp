#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>

namespace off::graphics {

// A descriptor-shaped subset of the values observed at the native MatPos
// clock-to-coordinate call site. This deliberately is not a MatPos player,
// transform, lifecycle service, or renderer input.
struct MatPosClockRange final {
    float lower_endpoint{};
    float upper_endpoint{};
    float rate{};
    bool control_loop{};
};

// KEYS packing metadata is independent from the retained animation endpoints.
// The observed call site uses these only when it turns a sampled frame into a
// packed coordinate.
struct MatPosKeysCoordinateDescriptor final {
    std::int32_t lower_frame{};
    std::int32_t count{};
    std::int32_t divisor{};
};

// Maps a pair of signed scene-clock snapshots to the narrow, observed packed
// coordinate calculation. It is a pure, read-only helper: callers own clock
// sampling, origin capture, terminal handling, and all animation effects. A
// trace proves that a forward clear-control branch can return -1 after
// sampling, but its selection predicate is not yet recovered. Terminal
// handling therefore remains outside this mapper rather than being inferred
// from endpoint comparison.
//
// The binary32 operation order and the one-sided upper clamp follow the
// recovered call-site arithmetic. That only establishes behavior for this
// validated descriptor/tick domain; it does not establish general MatPos
// semantics or claim equivalence outside it. Inputs that would make the
// supported arithmetic ambiguous or overflow fail closed with std::nullopt.
[[nodiscard]] inline std::optional<float>
map_matpos_clock_to_coordinate(std::int32_t origin_tick, std::int32_t now_tick,
                               const MatPosClockRange& range,
                               const MatPosKeysCoordinateDescriptor& descriptor) noexcept {
    if (!std::isfinite(range.lower_endpoint) || !std::isfinite(range.upper_endpoint) ||
        !std::isfinite(range.rate) || descriptor.count <= 0 || descriptor.divisor <= 0) {
        return std::nullopt;
    }

    // `cvttss2si`-style truncation is only represented where its result can be
    // carried safely by this portable, detached mapper.
    const float truncated_rate = std::trunc(range.rate);
    if (truncated_rate < -2147483648.0F || truncated_rate >= 2147483648.0F) {
        return std::nullopt;
    }
    const auto rate_i = static_cast<std::int32_t>(truncated_rate);
    const auto elapsed = std::max<std::int64_t>(
        static_cast<std::int64_t>(now_tick) - static_cast<std::int64_t>(origin_tick), 0);

    // Preserve the evidenced binary32 multiply before truncation rather than
    // replacing it with an exact integer multiply.
    const float lower_scale = range.lower_endpoint > range.upper_endpoint
        ? std::trunc(range.lower_endpoint * 1024.0F)
        : std::trunc(range.lower_endpoint * -1024.0F);
    if (!std::isfinite(lower_scale) ||
        lower_scale < static_cast<float>(std::numeric_limits<std::int64_t>::min()) ||
        lower_scale > static_cast<float>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    const auto lower_fixed = static_cast<std::int64_t>(lower_scale);
    if (rate_i != 0 && elapsed > std::numeric_limits<std::int64_t>::max() /
                                      std::llabs(static_cast<std::int64_t>(rate_i))) {
        return std::nullopt;
    }
    const auto progress = elapsed * static_cast<std::int64_t>(rate_i);
    const bool reverse = range.lower_endpoint > range.upper_endpoint;
    if ((!reverse && ((lower_fixed > 0 && progress < std::numeric_limits<std::int64_t>::min() + lower_fixed) ||
                      (lower_fixed < 0 && progress > std::numeric_limits<std::int64_t>::max() + lower_fixed))) ||
        (reverse && ((progress > 0 && lower_fixed < std::numeric_limits<std::int64_t>::min() + progress) ||
                     (progress < 0 && lower_fixed > std::numeric_limits<std::int64_t>::max() + progress)))) {
        return std::nullopt;
    }
    const auto fixed = reverse ? lower_fixed - progress : progress - lower_fixed;

    float frame{};
    if (range.control_loop) {
        // Subtract and truncate in binary32 before moving into the checked
        // portable integer domain; the endpoints are not KEYS frame indices.
        const float truncated_extent = std::trunc(range.upper_endpoint - range.lower_endpoint);
        if (!std::isfinite(truncated_extent) || truncated_extent < -9007199254740992.0F ||
            truncated_extent > 9007199254740992.0F) {
            return std::nullopt;
        }
        const auto extent = static_cast<std::int64_t>(truncated_extent);
        if (extent == 0 || extent > std::numeric_limits<std::int64_t>::max() / 1024 ||
            extent < std::numeric_limits<std::int64_t>::min() / 1024) return std::nullopt;
        const auto modulus = extent * 1024;
        if (modulus == 0) return std::nullopt;
        const auto remainder = fixed % modulus;
        frame = (reverse ? range.upper_endpoint : range.lower_endpoint) +
                static_cast<float>(remainder) / 1024.0F;
    } else {
        frame = static_cast<float>(fixed) / 1024.0F;
    }
    if (!std::isfinite(frame)) return std::nullopt;

    const float raw = (frame - static_cast<float>(descriptor.lower_frame)) /
                      static_cast<float>(descriptor.divisor);
    if (!std::isfinite(raw)) return std::nullopt;
    // The native call site clamps only the upper edge. Do not add a convenient
    // lower clamp: negative output is evidence-bearing behavior here.
    const float coordinate = std::min(static_cast<float>(descriptor.count - 1), raw);
    return coordinate;
}

} // namespace off::graphics
