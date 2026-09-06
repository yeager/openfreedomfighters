#include "off/graphics/bounds_propagation.hpp"

#include <array>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation) {
    bool caught = false;
    try { operation(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, "unsupported propagation rejects");
}
constexpr std::array<float, 9> identity{0, 0, 1, 0, 1, 0, 1, 0, 0};
struct Tree {
    std::array<PositionResource, 3> resources{{{17, 0x400}, {23, 0x800}, {41, 0x20}}};
    std::array<ResourceBounds, 3> bounds{{{{3, 4, 0}, {3, 4, 0}, 6},
                                        {{0, 0, 0}, {0, 0, 0}, 0},
                                        {{0, 0, 0}, {0, 0, 0}, 0}}};
    std::array<OwnerBounds, 3> owners{};
    std::array<BoundsNode, 3> nodes{};
    Tree() {
        for (std::size_t i = 0; i < nodes.size(); ++i)
            nodes[i] = {&resources[i], &bounds[i], &owners[i],
                        i + 1 < nodes.size() ? &nodes[i + 1] : nullptr, identity, {0, 0, 0}, false};
    }
};
}

int main() {
    static_assert(!std::is_copy_constructible_v<IdentityBoundsPropagation> &&
                  !std::is_move_constructible_v<IdentityBoundsPropagation> &&
                  !std::is_copy_assignable_v<IdentityBoundsPropagation> &&
                  !std::is_move_assignable_v<IdentityBoundsPropagation>);
    const int saved = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    {
        Tree t;
        t.nodes[0].position = {10, -2, 1};
        t.nodes[0].basis[0] = -0.0F;
        t.nodes[0].owner_bounds = nullptr; t.nodes[0].owner_opt_out = true;
        const auto result = get_identity_parent_bounds(t.nodes[0], -1);
        check(result && result->center == std::array<float, 3>{13, 2, 1} &&
              result->extents == std::array<float, 3>{3, 4, 0},
              "getter uses local translation, accepts signed-zero identity, ignores opt-out and owner output storage");
        t.bounds[0].center[0] = 16777216; t.nodes[0].position[0] = 1;
        check(get_identity_parent_bounds(t.nodes[0], 0)->center[0] == 16777216,
              "translation has a binary32 rounding boundary");
        BoundsNode absent{};
        check(!get_identity_parent_bounds(absent, 1), "suppression stops before live pointer access");
        rejects([&] { (void)get_identity_parent_bounds(absent, 0); });
        t.resources[0].flags |= 0x40000U; t.nodes[0].bounds = nullptr;
        check(!get_identity_parent_bounds(t.nodes[0], 0), "resource flag stops before bounds access");
        t.nodes[0].bounds = &t.bounds[0]; t.resources[0].flags = 0;
        t.bounds[0].radius = -0.0F; t.nodes[0].basis.fill(std::numeric_limits<float>::quiet_NaN());
        check(!get_identity_parent_bounds(t.nodes[0], 0), "numeric zero radius stops before unused transform validation");
        t.bounds[0].radius = std::numeric_limits<float>::quiet_NaN(); t.nodes[0].parent = nullptr;
        check(!get_identity_parent_bounds(t.nodes[0], 0), "no parent stops before unused nonfinite radius validation");
    }
    {
        Tree t;
        t.nodes[0].position = {10, 0, 0}; t.nodes[1].position = {100, 0, 0};
        t.nodes[2].basis.fill(std::numeric_limits<float>::quiet_NaN());
        t.bounds[1].center.fill(std::numeric_limits<float>::quiet_NaN());
        t.bounds[1].extents.fill(std::numeric_limits<float>::infinity());
        IdentityBoundsPropagation propagation;
        check(propagation.apply(t.nodes[0], 0) == 2, "two admitted parents update without fabricated initial bounds");
        check(t.bounds[1].center == std::array<float, 3>{13, 4, 0} &&
              t.bounds[2].center == std::array<float, 3>{113, 4, 0} &&
              t.bounds[1].extents == std::array<float, 3>{3, 4, 0x1p-13F} &&
              t.owners[1].extents == t.bounds[1].extents && t.owners[2].center == t.bounds[2].center &&
              t.resources[1].flags == 0x100800U && t.resources[2].flags == 0x100020U &&
              t.resources[0].flags == 0x400U, "shared writes preserve flags and copy clamped owner bounds");
        const auto old = t.bounds[1];
        t.bounds[0].extents = {1, 1, 0};
        check(propagation.apply(t.nodes[0], 0) == 0 && t.bounds[1].center == old.center &&
              t.bounds[1].extents == old.extents, "incremental update never contracts enclosing parent");
    }
    {
        Tree t;
        t.bounds[1] = {{0, 0, 0}, {100, 100, 100}, 200};
        t.nodes[1].owner_bounds = nullptr; t.nodes[1].parent = &t.nodes[1];
        t.nodes[1].basis.fill(std::numeric_limits<float>::quiet_NaN());
        IdentityBoundsPropagation propagation;
        check(propagation.apply(t.nodes[0], 0) == 0 && !propagation.failed() && t.resources[1].flags == 0x800U,
              "no expansion stops before owner output, farther cycle or unused parent transform inspection");
        t.bounds[1].radius = 0;
        rejects([&] { (void)propagation.apply(t.nodes[0], 0); });
        check(propagation.failed() && t.resources[1].flags == 0x800U,
              "updated parent requires owner storage before any parent writes");
    }
    for (int stop = 0; stop < 4; ++stop) {
        Tree t;
        t.nodes[0].basis.fill(std::numeric_limits<float>::quiet_NaN());
        if (stop == 0) t.resources[0].flags |= 0x40000U;
        if (stop == 1) t.nodes[0].owner_opt_out = true;
        if (stop == 2) t.nodes[0].parent = nullptr;
        if (stop == 3) t.bounds[0].radius = 0;
        IdentityBoundsPropagation propagation;
        check(propagation.apply(t.nodes[0], 0) == 0 && !propagation.failed(), "eligibility stops before transform checks");
    }
    {
        BoundsNode missing{};
        IdentityBoundsPropagation propagation;
        check(propagation.apply(missing, 1) == 0 && !propagation.failed(), "suppression stops without a live node payload");
        Tree t;
        t.resources[1].flags |= 0x40000U;
        check(propagation.apply(t.nodes[0], 0) == 1 && t.resources[2].flags == 0x20U,
              "parent receives incoming update before its own flag stops further propagation");
    }
    for (int invalid = 0; invalid < 5; ++invalid) {
        Tree t;
        if (invalid == 0) t.nodes[0].basis[0] = 1;
        if (invalid == 1) t.bounds[0].radius = -1;
        if (invalid == 2) t.bounds[0].extents[1] = -1;
        if (invalid == 3) t.nodes[0].position[2] = std::numeric_limits<float>::infinity();
        if (invalid == 4) t.bounds[0].extents[0] = 1.0e30F;
        IdentityBoundsPropagation propagation;
        rejects([&] { (void)propagation.apply(t.nodes[0], 0); });
        check(propagation.failed() && t.resources[1].flags == 0x800U && t.bounds[1].radius == 0,
              "consumed invalid state rejects before parent writes and poisons");
    }
    {
        Tree t;
        t.nodes[1].basis[0] = 1;
        IdentityBoundsPropagation propagation;
        rejects([&] { (void)propagation.apply(t.nodes[0], 0); });
        check(propagation.failed() && t.resources[1].flags == 0x100800U && t.bounds[1].radius == 6 &&
              t.resources[2].flags == 0x20U, "later invalid transform preserves earlier parent update prefix");
        rejects([&] { (void)propagation.apply(t.nodes[0], 1); });
        Tree cyclic;
        cyclic.nodes[1].parent = &cyclic.nodes[0];
        IdentityBoundsPropagation cycle;
        rejects([&] { (void)cycle.apply(cyclic.nodes[0], 0); });
        check(cycle.failed() && cyclic.resources[1].flags == 0x100800U && cyclic.resources[0].flags == 0x400U,
              "reached cycle rejects after retained prefix and before rewriting repeated node");
    }
    for (bool resource_alias : {false, true}) {
        Tree t;
        if (resource_alias) t.nodes[1].resource = t.nodes[0].resource;
        else t.nodes[1].bounds = t.nodes[0].bounds;
        IdentityBoundsPropagation propagation;
        rejects([&] { (void)propagation.apply(t.nodes[0], 0); });
        check(propagation.failed() && t.resources[0].flags == 0x400U && t.resources[1].flags == 0x800U &&
              t.bounds[0].center == std::array<float, 3>{3, 4, 0} && t.bounds[1].radius == 0,
              "distinct node wrapper cannot alias a reached live resource or bounds storage");
        Tree later;
        if (resource_alias) later.nodes[2].resource = later.nodes[0].resource;
        else later.nodes[2].bounds = later.nodes[0].bounds;
        IdentityBoundsPropagation partial;
        rejects([&] { (void)partial.apply(later.nodes[0], 0); });
        check(partial.failed() && later.resources[1].flags == 0x100800U && later.bounds[1].radius == 6 &&
              later.resources[0].flags == 0x400U && later.resources[2].flags == 0x20U,
              "later storage alias rejection retains earlier completed ancestor prefix");
        Tree stopped;
        stopped.bounds[1] = {{0, 0, 0}, {100, 100, 100}, 200};
        if (resource_alias) stopped.nodes[2].resource = stopped.nodes[0].resource;
        else stopped.nodes[2].bounds = stopped.nodes[0].bounds;
        IdentityBoundsPropagation unused;
        check(unused.apply(stopped.nodes[0], 0) == 0 && !unused.failed() && stopped.resources[1].flags == 0x800U,
              "unreached storage alias behind no-expansion stop remains uninspected");
    }
    for (int mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0) {
            Tree t;
            rejects([&] { (void)get_identity_parent_bounds(t.nodes[0], 0); });
            check(!get_identity_parent_bounds(t.nodes[0], 1), "rounding policy does not bypass early suppression");
        }
    }
    check(std::fesetround(saved) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
