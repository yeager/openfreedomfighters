#pragma once

#include "off/data/matpos_pose_evaluator.hpp"
#include "off/runtime/owner_component_provider_binding.hpp"

#include <array>
#include <cmath>

namespace off::runtime {

// The recovered MatPos consumer is a typed owner/provider service. It is not
// a general scene-transform API: the native call receives transient local
// basis and translation values and does not expose or consume a return value.
class MatPosOwnerTransformProvider : public OwnerComponentProvider {
public:
    virtual void apply_matpos_local_transform(
        const std::array<float, 9>& basis,
        const std::array<float, 3>& translation) noexcept = 0;
};

// Couples a MatPos receiver to the same enrolled provider lifetime as its
// exact KEYS child lookup. The binding never retains a sampled pose and fails
// closed when the owner is gone or cannot presently resolve KEYS.
class MatPosOwnerTransformApplyBinding final {
public:
    MatPosOwnerTransformApplyBinding() = default;
    MatPosOwnerTransformApplyBinding(const MatPosOwnerTransformApplyBinding&) = delete;
    MatPosOwnerTransformApplyBinding& operator=(const MatPosOwnerTransformApplyBinding&) = delete;

    void bind(MatPosOwnerTransformProvider& provider,
              OwnerComponentProviderBindings& bindings) {
        if (provider_ != nullptr || binding_.bound() || binding_.invalidated()) {
            throw std::runtime_error("MatPos owner transform binding cannot be rebound");
        }
        binding_.bind(provider, bindings);
        provider_ = &provider;
    }

    void invalidate() noexcept {
        binding_.invalidate();
        provider_ = nullptr;
    }

    [[nodiscard]] bool bound() const noexcept {
        return provider_ != nullptr && binding_.bound();
    }
    [[nodiscard]] bool invalidated() const noexcept { return binding_.invalidated(); }

    // This is intentionally void: the recovered caller ignores the target
    // call's result. No owner/resource/render identity is accepted here.
    void apply_if_live(const data::DetachedMatPosPose& pose) const noexcept {
        if (!bound() || binding_.find_required_keys_child() == nullptr ||
            !finite(pose.basis) || !finite(pose.translation)) {
            return;
        }
        provider_->apply_matpos_local_transform(pose.basis, pose.translation);
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

    OwnerComponentProviderBinding binding_;
    MatPosOwnerTransformProvider* provider_{};
};

} // namespace off::runtime
