#include "off/runtime/matpos_owner_transform_apply.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace off;

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <class F>
void rejects(F&& operation) {
    bool caught = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught, "expected binding rejection");
}

struct Provider final : runtime::MatPosOwnerTransformProvider {
    const void* child{this};
    std::size_t queries{};
    std::size_t calls{};
    std::array<float, 9> received_basis{};
    std::array<float, 3> received_translation{};

    [[nodiscard]] const void* find_child_exact(std::array<char, 4> key) const noexcept override {
        auto* self = const_cast<Provider*>(this);
        ++self->queries;
        return key == std::array<char, 4>{'K', 'E', 'Y', 'S'} ? child : nullptr;
    }

    void apply_matpos_local_transform(const std::array<float, 9>& basis,
                                      const std::array<float, 3>& translation) noexcept override {
        ++calls;
        received_basis = basis;
        received_translation = translation;
    }
};

static_assert(std::is_same_v<decltype(&runtime::MatPosOwnerTransformProvider::apply_matpos_local_transform),
                             void (runtime::MatPosOwnerTransformProvider::*)(
                                 const std::array<float, 9>&,
                                 const std::array<float, 3>&) noexcept>);

data::DetachedMatPosPose pose() {
    return {{0, 0, 0, 1}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {10, 11, 12}};
}
}

int main() {
    try {
        runtime::OwnerComponentProviderBindings enrolled;
        Provider provider;
        runtime::MatPosOwnerTransformApplyBinding binding;
        const auto sample = pose();

        binding.apply_if_live(sample);
        check(provider.calls == 0 && provider.queries == 0,
              "unbound MatPos owner binding fails closed");

        binding.bind(provider, enrolled);
        check(binding.bound() && !binding.invalidated() && enrolled.size() == 1,
              "typed MatPos binding enrolls its provider once");
        binding.apply_if_live(sample);
        check(provider.calls == 1 && provider.queries == 1 &&
                  provider.received_basis == sample.basis &&
                  provider.received_translation == sample.translation,
              "live typed provider receives one exact transient local pose");
        binding.apply_if_live(sample);
        check(provider.calls == 2 && provider.queries == 2,
              "each accepted apply performs a fresh KEYS lookup");

        provider.child = nullptr;
        binding.apply_if_live(sample);
        check(provider.calls == 2 && provider.queries == 3,
              "missing live KEYS child suppresses transform application");
        provider.child = &provider;

        auto non_finite_basis = sample;
        non_finite_basis.basis[3] = std::numeric_limits<float>::infinity();
        binding.apply_if_live(non_finite_basis);
        auto non_finite_translation = sample;
        non_finite_translation.translation[1] = std::numeric_limits<float>::quiet_NaN();
        binding.apply_if_live(non_finite_translation);
        check(provider.calls == 2 && provider.queries == 5,
              "non-finite local pose inputs fail closed after fresh liveness checks");

        rejects([&] { binding.bind(provider, enrolled); });
        enrolled.invalidate_all();
        check(!binding.bound() && binding.invalidated() && enrolled.size() == 0,
              "provider teardown invalidates the typed apply binding");
        binding.apply_if_live(sample);
        check(provider.calls == 2 && provider.queries == 5,
              "invalidated provider cannot be queried or applied");
        rejects([&] { binding.bind(provider, enrolled); });

        runtime::MatPosOwnerTransformApplyBinding explicit_invalidation;
        explicit_invalidation.bind(provider, enrolled);
        explicit_invalidation.invalidate();
        check(!explicit_invalidation.bound() && explicit_invalidation.invalidated() && enrolled.size() == 0,
              "explicit invalidation removes the enrolled typed binding");
        std::cout << "MatPos owner transform application remains typed, transient and fail-closed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
