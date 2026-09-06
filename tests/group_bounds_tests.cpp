#include "off/graphics/group_bounds.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class F> void rejects(F operation) {
    bool caught = false;
    try { operation(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, "unsupported group recomputation rejects");
}
struct Fixture {
    GroupBoundsRecompute group;
    ResourceBounds resource{{11, 12, 13}, {14, 15, 16}, 17};
    OwnerBounds owner{{21, 22, 23}, {24, 25, 26}};
    std::uint32_t flags = 0x400U;
    std::array<PositionResource, 7> resources{};
    std::array<GroupBoundsChild, 7> children{};
    std::vector<std::string> calls;
    GroupBoundsHooks hooks;
    Fixture() {
        for (std::size_t i = 0; i < children.size(); ++i) {
            resources[i] = {i, 0}; children[i] = {&resources[i], true};
        }
        hooks.opt_out = [&](const PositionResource& r) { record("o", r); return false; };
        hooks.name = [&](const PositionResource& r) -> std::optional<std::string_view> {
            record("n", r); return std::nullopt;
        };
        hooks.get_bounds = [&](PositionResource& r) -> std::optional<ParentSpaceBounds> {
            record("b", r); return ParentSpaceBounds{{3, 4, 0}, {3, 4, 0}};
        };
    }
    bool unchanged() const {
        return resource.center == std::array<float, 3>{11, 12, 13} &&
               resource.extents == std::array<float, 3>{14, 15, 16} && resource.radius == 17 &&
               owner.center == std::array<float, 3>{21, 22, 23} &&
               owner.extents == std::array<float, 3>{24, 25, 26} && flags == 0x400U;
    }
    void record(const char* prefix, const PositionResource& r) {
        check(unchanged(), "all child callbacks precede every group resource and owner write");
        calls.push_back(std::string(prefix) + std::to_string(r.identity));
    }
};
}

int main() {
    static_assert(!std::is_copy_constructible_v<GroupBoundsRecompute> &&
                  !std::is_move_constructible_v<GroupBoundsRecompute> &&
                  !std::is_copy_assignable_v<GroupBoundsRecompute> &&
                  !std::is_move_assignable_v<GroupBoundsRecompute>);
    const int saved = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    constexpr float minimum = 0x1p-13F;
    {
        Fixture f;
        f.resources[0].flags = 0x40000U;
        f.resources[4].flags = 0xC00U;
        f.children[5].owner_present = false;
        f.hooks.opt_out = [&](const PositionResource& r) { f.record("o", r); return r.identity == 1; };
        f.hooks.name = [&](const PositionResource& r) -> std::optional<std::string_view> {
            f.record("n", r);
            if (r.identity == 2) return "MOVETOCREATION";
            if (r.identity == 4) return "movetocreation";
            return std::nullopt;
        };
        f.hooks.get_bounds = [&](PositionResource& r) -> std::optional<ParentSpaceBounds> {
            f.record("b", r);
            if (r.identity == 3) return std::nullopt;
            return ParentSpaceBounds{{3, 4, 0}, {3, 4, 0}};
        };
        f.group.apply(f.resource, f.flags, f.owner, f.children, true, 1, f.hooks);
        check(f.calls == std::vector<std::string>{"o1", "o2", "n2", "o3", "n3", "b3",
              "o4", "n4", "b4", "b5", "o6", "n6", "b6"},
              "guard order preserves runtime filtering, missing owner, exact case and false getter semantics");
        check(f.resource.center == std::array<float, 3>{3, 4, 0} &&
              f.resource.extents == std::array<float, 3>{3, 4, minimum} && f.resource.radius == 6 &&
              f.owner.center == f.resource.center && f.owner.extents == std::array<float, 3>{3, 4, 0} &&
              f.flags == 0x100400U, "group contracts prior bounds, clamps only resource and preserves hidden flag");
    }
    for (bool parent : {false, true}) for (std::uint32_t status : {0U, 1U, 2U, 3U}) {
        Fixture f;
        const std::array<GroupBoundsChild, 0> empty{};
        f.group.apply(f.resource, f.flags, f.owner, empty, parent, status, f.hooks);
        const bool fallback = !parent && (status & 1U) != 0;
        check(f.resource.center == std::array<float, 3>{0, 0, 0} &&
              f.resource.extents == (fallback ? std::array<float, 3>{2500, minimum, 2500} :
                                               std::array<float, 3>{minimum, minimum, minimum}) &&
              (fallback ? f.resource.radius > 3500 : f.resource.radius == 0) &&
              f.owner.center == std::array<float, 3>{0, 0, 0} && f.owner.extents == std::array<float, 3>{0, 0, 0},
              "conditional root fallback contributes only to resource union; true empty radius is zero");
    }
    {
        Fixture f;
        f.group.apply(f.resource, f.flags, f.owner, std::span(f.children).first(1), false, 1, f.hooks);
        check(f.resource.center == std::array<float, 3>{0, 4, 0} &&
              f.resource.extents == std::array<float, 3>{2500, 4, 2500} &&
              f.owner.center == std::array<float, 3>{3, 4, 0} && f.owner.extents == std::array<float, 3>{3, 4, 0},
              "child-only owner union excludes seeded root dimensions");
    }
    {
        Fixture f;
        const float negative_sentinel = -std::bit_cast<float>(0x7e967699U);
        f.hooks.get_bounds = [&](PositionResource&) -> std::optional<ParentSpaceBounds> {
            return ParentSpaceBounds{{negative_sentinel, 100, 200}, {0, 1, 2}};
        };
        f.group.apply(f.resource, f.flags, f.owner, std::span(f.children).first(1), true, 0, f.hooks);
        check(f.resource.radius == 0 && f.owner.center == std::array<float, 3>{0, 0, 0},
              "upper X equal to original sentinel means empty even after successful nonempty Y/Z visits");
        Fixture g;
        g.hooks.get_bounds = [](PositionResource&) -> std::optional<ParentSpaceBounds> {
            return ParentSpaceBounds{{16777216, 0, 0}, {1, 0, 0}};
        };
        g.group.apply(g.resource, g.flags, g.owner, std::span(g.children).first(1), true, 0, g.hooks);
        check(g.resource.center[0] == 16777216 && g.owner.extents[0] == 0 && g.resource.radius == 1,
              "binary32 corners and center rounding precede upper-minus-center extent calculation");
    }
    {
        Fixture f;
        auto invalid = f.children; invalid[6].resource = nullptr;
        rejects([&] { f.group.apply(f.resource, f.flags, f.owner, invalid, true, 0, f.hooks); });
        for (int hook = 0; hook < 3; ++hook) {
            auto missing = f.hooks;
            if (hook == 0) missing.opt_out = {};
            if (hook == 1) missing.name = {};
            if (hook == 2) missing.get_bounds = {};
            rejects([&] { f.group.apply(f.resource, f.flags, f.owner, f.children, true, 0, missing); });
        }
        for (int mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO})
            if (std::fesetround(mode) == 0)
                rejects([&] { f.group.apply(f.resource, f.flags, f.owner, f.children, true, 0, f.hooks); });
        check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding");
        check(f.unchanged() && f.calls.empty() && !f.group.failed(), "prevalidation rejects before any hook without poisoning");
    }
    for (int invalid = 0; invalid < 5; ++invalid) {
        Fixture f;
        f.hooks.get_bounds = [&](PositionResource& r) -> std::optional<ParentSpaceBounds> {
            f.record("b", r); r.flags |= 0x80U;
            if (invalid == 0) throw std::runtime_error("external getter");
            ParentSpaceBounds b{{0, 0, 0}, {1, 1, 1}};
            if (invalid == 1) b.center[2] = std::numeric_limits<float>::quiet_NaN();
            if (invalid == 2) b.extents[1] = -1;
            if (invalid == 3) b.extents[0] = std::numeric_limits<float>::infinity();
            if (invalid == 4) b.extents[0] = 1.0e30F;
            return b;
        };
        rejects([&] { f.group.apply(f.resource, f.flags, f.owner, std::span(f.children).first(1), true, 0, f.hooks); });
        check(f.unchanged() && f.group.failed() && f.resources[0].flags == 0x80U,
              "getter or arithmetic failure retains external prefix but no group writes");
        const auto count = f.calls.size();
        rejects([&] { f.group.apply(f.resource, f.flags, f.owner, f.children, true, 0, f.hooks); });
        check(f.calls.size() == count, "poisoned recomputation cannot retry callbacks");
    }
    {
        Fixture f;
        f.hooks.get_bounds = [&](PositionResource& r) -> std::optional<ParentSpaceBounds> {
            f.record("b", r);
            rejects([&] { f.group.apply(f.resource, f.flags, f.owner, f.children, true, 0, f.hooks); });
            return std::nullopt;
        };
        f.group.apply(f.resource, f.flags, f.owner, std::span(f.children).first(1), true, 0, f.hooks);
        check(!f.group.failed() && f.resource.radius == 0, "caught reentry rejects and false getter contributes nothing");
    }
    check(std::fesetround(saved) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
