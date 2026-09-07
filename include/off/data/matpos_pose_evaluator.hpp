#pragma once

#include "off/data/keys_backing_evaluator.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace off::data {

// A sampled MatPos pose deliberately has no owner, resource handle, or scene
// side effect. Component order is retained exactly as stored by KEYS; its
// quaternion-to-basis convention has not been recovered yet.
struct DetachedMatPosPose final {
    std::array<float, 4> normalized_orientation{};
    std::array<float, 3> translation{};
};

// Converts a previously bounded KEYS numeric sample into a detached local pose.
// The four orientation values are signed quantized components. The original
// path linearly blends them before this boundary; normalizing them before
// interpolation would therefore be incorrect. Their component ordering and
// basis convention remain deliberately opaque here.
class MatPosPoseEvaluator final {
public:
    [[nodiscard]] static std::optional<DetachedMatPosPose> evaluate(
        const KeysBackingSample& sample) noexcept {
        if (!finite(sample.first_group) || !finite(sample.second_group)) {
            return std::nullopt;
        }

        constexpr double quantization_scale = 1.0 / 32512.0;
        std::array<double, 4> orientation{};
        double length_squared{};
        for (std::size_t index = 0; index < orientation.size(); ++index) {
            orientation[index] = static_cast<double>(sample.first_group[index]) * quantization_scale;
            length_squared += orientation[index] * orientation[index];
        }
        if (!std::isfinite(length_squared) || length_squared <= 0.0) {
            return std::nullopt;
        }

        const double inverse_length = 1.0 / std::sqrt(length_squared);
        if (!std::isfinite(inverse_length)) {
            return std::nullopt;
        }
        DetachedMatPosPose result{
            .normalized_orientation = {
                static_cast<float>(orientation[0] * inverse_length),
                static_cast<float>(orientation[1] * inverse_length),
                static_cast<float>(orientation[2] * inverse_length),
                static_cast<float>(orientation[3] * inverse_length),
            },
            .translation = sample.second_group,
        };
        return finite(result.normalized_orientation) && finite(result.translation)
                   ? std::optional(result)
                   : std::nullopt;
    }

private:
    template <std::size_t N>
    [[nodiscard]] static bool finite(const std::array<float, N>& values) noexcept {
        for (const float value : values) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        return true;
    }
};

} // namespace off::data
