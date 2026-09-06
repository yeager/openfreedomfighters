#include "off/graphics/picture_bounds.hpp"

#include <array>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* text) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported bounds application rejects");
}
const std::array<off::data::PictureResourceDescriptor, 1> descriptors{[] {
    off::data::PictureResourceDescriptor d;
    d.local_center_x = 3; d.local_center_y = 4;
    d.horizontal_edge_span = 6; d.vertical_edge_span = 8;
    return d;
}()};
const std::array<off::data::PictureDrawGroup, 1> groups{{{1, 0}}};
bool final_bounds(const ResourceBounds& b) {
    return b.center == std::array<float, 3>{3, -4, 0} &&
           b.extents == std::array<float, 3>{3, 4, 0x1p-13F} && b.radius == 6;
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<PictureBoundsApplication> &&
                  !std::is_move_constructible_v<PictureBoundsApplication> &&
                  !std::is_copy_assignable_v<PictureBoundsApplication> &&
                  !std::is_move_assignable_v<PictureBoundsApplication>);
    const int saved = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    for (bool success : {false, true}) {
        PictureBoundsApplication app;
        ResourceBounds bounds{{11, 12, 13}, {14, 15, 16}, 17};
        std::uint32_t flags = 0x400U;
        int calls = 0;
        const auto query = [&](std::uint64_t identity, std::uint64_t renderer,
                               std::array<float, 3>& center, std::array<float, 3>& extent) {
            ++calls;
            check(identity == 23 && renderer == 9001 && &center == &bounds.center && &extent == &bounds.extents,
                  "renderer query receives distinct actual IDs and live output storage");
            if (calls == 1)
                check(center == std::array<float, 3>{11, 12, 13} && extent == std::array<float, 3>{14, 15, 16} &&
                      flags == 0x400U && bounds.radius == 17, "no runtime writes precede required renderer query");
            else check(final_bounds(bounds), "repeated call queries current descriptor-derived state");
            center = {101, 102, 103}; extent = {0, -2, 0};
            if (!success) {
                center[0] = std::numeric_limits<float>::quiet_NaN();
                extent[1] = std::numeric_limits<float>::infinity();
            }
            return success;
        };
        app.apply(bounds, flags, 23, 9001, descriptors, groups, {1, 1}, query);
        check(final_bounds(bounds) && flags == 0x100400U && !app.failed(),
              "descriptor bounds replace both renderer branches preserving unrelated flags");
        app.apply(bounds, flags, 23, 9001, descriptors, groups, {1, 1}, query);
        check(calls == 2 && final_bounds(bounds), "successful application has no once-only latch");
    }
    {
        PictureBoundsApplication app;
        ResourceBounds bounds{{1, 2, 3}, {4, 5, 6}, 7};
        std::uint32_t flags = 0x20U;
        int calls = 0;
        const auto query = [&](std::uint64_t, std::uint64_t, auto&, auto&) { ++calls; return false; };
        rejects([&] { app.apply(bounds, flags, 0, 0, descriptors, groups, {1, 1}, {}); });
        auto invalid = descriptors; invalid[0].horizontal_edge_span = -1;
        rejects([&] { app.apply(bounds, flags, 0, 0, invalid, groups, {1, 1}, query); });
        rejects([&] { app.apply(bounds, flags, 0, 0, descriptors, groups, {0, 1}, query); });
        for (int rounding : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
            if (std::fesetround(rounding) == 0)
                rejects([&] { app.apply(bounds, flags, 0, 0, descriptors, groups, {1, 1}, query); });
        }
        check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding");
        check(!app.failed() && calls == 0 && flags == 0x20U && bounds.radius == 7 &&
              bounds.center == std::array<float, 3>{1, 2, 3} && bounds.extents == std::array<float, 3>{4, 5, 6},
              "prevalidation failures leave all state intact and do not poison");
        bounds.center[0] = std::numeric_limits<float>::infinity();
        bounds.extents[0] = std::numeric_limits<float>::quiet_NaN();
        app.apply(bounds, flags, 0, 0, descriptors, groups, {1, 1}, query);
        check(final_bounds(bounds) && calls == 1, "false query can replace nonfinite preexisting bounds");
    }
    for (int failure = 0; failure < 4; ++failure) {
        PictureBoundsApplication app;
        ResourceBounds bounds{{1, 2, 3}, {4, 5, 6}, 7};
        std::uint32_t flags = 0x80U;
        int calls = 0;
        const auto query = [&](std::uint64_t, std::uint64_t, auto& center, auto& extents) {
            ++calls; center[0] = 55;
            if (failure == 0) throw std::runtime_error("query failure");
            if (failure == 1) center[1] = std::numeric_limits<float>::quiet_NaN();
            if (failure == 2) extents[2] = std::numeric_limits<float>::infinity();
            if (failure == 3) extents[0] = 1.0e30F;
            return true;
        };
        rejects([&] { app.apply(bounds, flags, 1, 2, descriptors, groups, {1, 1}, query); });
        check(app.failed() && bounds.center[0] == 55 && bounds.radius == 7 && flags == 0x80U && calls == 1,
              "query or returned arithmetic failure retains live output prefix before dirty/radius writes");
        rejects([&] { app.apply(bounds, flags, 1, 2, descriptors, groups, {1, 1}, query); });
        check(calls == 1, "poisoned application cannot repeat query");
    }
    for (bool catch_nested : {false, true}) {
        PictureBoundsApplication app;
        ResourceBounds bounds{{1, 2, 3}, {4, 5, 6}, 7};
        std::uint32_t flags = 0;
        const auto query = [&](std::uint64_t, std::uint64_t, auto& center, auto&) {
            center[0] = 99;
            const auto nested = [&] { app.apply(bounds, flags, 0, 0, descriptors, groups, {1, 1},
                [](std::uint64_t, std::uint64_t, auto&, auto&) { return false; }); };
            if (catch_nested) rejects(nested); else nested();
            return false;
        };
        const auto outer = [&] { app.apply(bounds, flags, 0, 0, descriptors, groups, {1, 1}, query); };
        if (catch_nested) outer(); else rejects(outer);
        check(app.failed() != catch_nested && (catch_nested ? final_bounds(bounds) : bounds.center[0] == 99),
              "caught reentry permits outer completion; escaping reentry poisons with retained prefix");
    }
    check(std::fesetround(saved) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
