#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace off::runtime {

// A typed local notification boundary which deliberately has no resource,
// hierarchy, cache, or renderer identity.
class MatPosOwnerTransformInvalidation {
public:
    virtual ~MatPosOwnerTransformInvalidation() = default;
    virtual void invalidate_matpos_owner_local_transform() noexcept = 0;
};

// A detached owner-local transform state model.
// Equality follows its fast path (three translation words plus the first six
// basis words); a changed pose still copies the full 3x3 basis. The runtime
// does not bind this value object to a live hierarchy or position service.
class MatPosOwnerLocalTransformState final {
public:
    using Basis = std::array<float, 9>;
    using Translation = std::array<float, 3>;

    constexpr MatPosOwnerLocalTransformState(Basis basis,
                                             Translation translation) noexcept
        : basis_(basis), translation_(translation) {}

    void apply(const Basis& basis, const Translation& translation,
               MatPosOwnerTransformInvalidation& invalidation) noexcept {
        if (!finite(basis) || !finite(translation)) {
            return;
        }
        if (same_basis(basis_, basis) && same_translation(translation_, translation)) {
            return;
        }
        basis_ = basis;
        translation_ = translation;
        transform_dirty_ = true;
        invalidation.invalidate_matpos_owner_local_transform();
    }

    [[nodiscard]] constexpr const Basis& basis() const noexcept { return basis_; }
    [[nodiscard]] constexpr const Translation& translation() const noexcept { return translation_; }
    [[nodiscard]] constexpr bool transform_dirty() const noexcept { return transform_dirty_; }

private:
    [[nodiscard]] static bool same_basis(const Basis& left, const Basis& right) noexcept {
        // Compare the leading basis terms; accepted changes retain the full
        // basis so downstream consumers see a complete transform.
        for (std::size_t index = 0; index < 6; ++index) {
            if (std::bit_cast<std::uint32_t>(left[index]) !=
                std::bit_cast<std::uint32_t>(right[index])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static bool same_translation(const Translation& left,
                                               const Translation& right) noexcept {
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (std::bit_cast<std::uint32_t>(left[index]) !=
                std::bit_cast<std::uint32_t>(right[index])) {
                return false;
            }
        }
        return true;
    }

    template <std::size_t N>
    [[nodiscard]] static bool finite(const std::array<float, N>& values) noexcept {
        for (const float value : values) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        return true;
    }

    Basis basis_{};
    Translation translation_{};
    bool transform_dirty_{};
};

} // namespace off::runtime
