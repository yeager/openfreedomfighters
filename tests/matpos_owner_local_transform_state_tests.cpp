#include "off/runtime/matpos_owner_local_transform_state.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace off::runtime;

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Invalidation final : MatPosOwnerTransformInvalidation {
    MatPosOwnerLocalTransformState* state{};
    std::size_t calls{};
    bool observed_dirty{};
    MatPosOwnerLocalTransformState::Basis observed_basis{};
    MatPosOwnerLocalTransformState::Translation observed_translation{};

    void invalidate_matpos_owner_local_transform() noexcept override {
        ++calls;
        observed_dirty = state->transform_dirty();
        observed_basis = state->basis();
        observed_translation = state->translation();
    }
};

static_assert(std::is_same_v<decltype(&MatPosOwnerTransformInvalidation::invalidate_matpos_owner_local_transform),
                             void (MatPosOwnerTransformInvalidation::*)() noexcept>);

using State = MatPosOwnerLocalTransformState;
constexpr State::Basis identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr State::Translation origin{0, 0, 0};
}

int main() {
    try {
        State state{identity, origin};
        Invalidation invalidation;
        invalidation.state = &state;
        check(state.basis() == identity && state.translation() == origin && !state.transform_dirty(),
              "owner-local transform state starts clean with its supplied values");

        state.apply(identity, origin, invalidation);
        check(invalidation.calls == 0 && !state.transform_dirty(),
              "identical owner-local transform is an exact no-op");

        auto signed_zero_translation = origin;
        signed_zero_translation[1] = -0.0F;
        state.apply(identity, signed_zero_translation, invalidation);
        check(invalidation.calls == 0 && state.translation() == origin,
              "translation comparison treats signed zero as numerically equal");

        auto signed_zero_basis = identity;
        signed_zero_basis[1] = -0.0F;
        state.apply(signed_zero_basis, origin, invalidation);
        check(invalidation.calls == 1 && state.transform_dirty() &&
                  state.basis() == signed_zero_basis && state.translation() == origin &&
                  invalidation.observed_dirty && invalidation.observed_basis == signed_zero_basis &&
                  invalidation.observed_translation == origin,
              "basis word changes commit complete state and dirty it before notification");

        const State::Translation moved{2, 4, 8};
        state.apply(signed_zero_basis, moved, invalidation);
        check(invalidation.calls == 2 && state.basis() == signed_zero_basis &&
                  state.translation() == moved && invalidation.observed_translation == moved,
              "translation changes copy the complete retained owner-local transform");

        const auto retained_basis = state.basis();
        const auto retained_translation = state.translation();
        auto non_finite_basis = retained_basis;
        non_finite_basis[5] = std::numeric_limits<float>::infinity();
        state.apply(non_finite_basis, moved, invalidation);
        auto non_finite_translation = moved;
        non_finite_translation[0] = std::numeric_limits<float>::quiet_NaN();
        state.apply(retained_basis, non_finite_translation, invalidation);
        check(invalidation.calls == 2 && state.basis() == retained_basis &&
                  state.translation() == retained_translation,
              "non-finite samples cannot write owner-local state or notify");

        const State::Basis second_basis{0, 1, 0, 1, 0, 0, 0, 0, 1};
        state.apply(second_basis, moved, invalidation);
        check(invalidation.calls == 3 && invalidation.observed_basis == second_basis,
              "each distinct accepted transform synchronously notifies once");
        std::cout << "MatPos owner-local transform state commits and dirties without renderer admission.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
