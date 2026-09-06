#include "off/graphics/picture_bounds.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using off::data::PictureDrawGroup;
using off::data::PictureResourceDescriptor;
using off::graphics::compute_picture_bounds;
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported bounds input rejects");
}
PictureResourceDescriptor descriptor(float x, float y, float width, float height) {
    PictureResourceDescriptor result;
    result.local_center_x = x; result.local_center_y = y;
    result.horizontal_edge_span = width; result.vertical_edge_span = height;
    return result;
}
}

int main() {
    const int saved_rounding = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    const std::array<PictureDrawGroup, 1> one{{{1, 0}}};
    const float minimum = 0x1p-13F;
    {
        std::array<PictureResourceDescriptor, 1> data{descriptor(3, 4, 6, 8)};
        data[0].local_z = std::numeric_limits<float>::infinity();
        data[0].u_min = std::numeric_limits<float>::quiet_NaN();
        data[0].v_max = -std::numeric_limits<float>::infinity();
        const auto result = compute_picture_bounds(data, one, {1, 1});
        check(result.center == std::array<float, 3>{3, -4, 0} &&
              result.raw_extents == std::array<float, 3>{3, 4, 0} &&
              result.extents == std::array<float, 3>{3, 4, minimum} && result.radius == 6,
              "planar bounds flip Y center, ignore Z/UV, clamp extents and use raw 3-4-5 radius");
        const auto scaled = compute_picture_bounds(data, one, {2, 3});
        check(scaled.center == std::array<float, 3>{6, -12, 0} &&
              scaled.raw_extents == std::array<float, 3>{6, 12, 0}, "explicit axis scales apply independently");
        const volatile float root = static_cast<float>(std::sqrt(180.0));
        check(scaled.radius == root + 1.0F, "portable radius narrows square root before adding one");
    }
    {
        std::array<PictureResourceDescriptor, 3> data{
            descriptor(-4, -2, 4, 2), descriptor(5, 3, 2, 4), descriptor(900, 900, 1, 1)};
        data[2].local_center_x = std::numeric_limits<float>::quiet_NaN();
        const std::array<PictureDrawGroup, 4> groups{{{1, 1}, {2, 0}, {1, 0}, {0, 3}}};
        const auto result = compute_picture_bounds(data, groups, {1, 1});
        check(result.center == std::array<float, 3>{0, -1, 0} &&
              result.raw_extents == std::array<float, 3>{6, 4, 0},
              "group spans route selected descriptors with overlaps and empty tail span");
        const std::array<PictureDrawGroup, 1> direct{{{2, 0}}};
        const auto unique = compute_picture_bounds(data, direct, {1, 1});
        check(result.center == unique.center && result.extents == unique.extents && result.radius == unique.radius,
              "duplicate descriptor visits preserve geometric union");
    }
    {
        std::array<PictureResourceDescriptor, 1> data{descriptor(0, 0, 0, 0)};
        const auto result = compute_picture_bounds(data, one, {1, 1});
        check(result.raw_extents == std::array<float, 3>{0, 0, 0} &&
              result.extents == std::array<float, 3>{minimum, minimum, minimum} && result.radius == 1,
              "zero raw extents give radius one rather than using clamp thickness");
        check(std::bit_cast<std::uint32_t>(result.center[1]) == 0x80000000U &&
              std::bit_cast<std::uint32_t>(result.center[2]) == 0,
              "Y sign-bit negation preserves negative zero and descriptor Z is positive zero");
        data[0] = descriptor(0, 0, 2 * minimum, minimum);
        const auto threshold = compute_picture_bounds(data, one, {1, 1});
        check(threshold.raw_extents[0] == minimum && threshold.raw_extents[1] == minimum * 0.5F &&
              threshold.extents[0] == minimum && threshold.extents[1] == minimum,
              "strict clamp preserves equal extent and raises only lower values");
        data[0] = descriptor(16777216.0F, 0, 2, 0);
        const auto rounded = compute_picture_bounds(data, one, {2, 1});
        check(rounded.center[0] == 33554432.0F && rounded.raw_extents[0] == 1 && rounded.radius == 2,
              "binary32 corner rounding precedes union difference and scaling");
        for (float x : {2.0e38F, -2.0e38F}) {
            data[0] = descriptor(x, 0, 0, 0);
            const auto sentinel = compute_picture_bounds(data, one, {0x1p-100F, 1});
            check(sentinel.raw_extents[0] > 0 && std::isfinite(sentinel.radius),
                  "finite sentinel retained for extreme endpoints instead of first-descriptor initialization");
        }
    }
    {
        std::array<PictureResourceDescriptor, 1> data{descriptor(0, 0, 1, 1)};
        const std::array<PictureDrawGroup, 0> empty{};
        rejects([&] { (void)compute_picture_bounds(data, empty, {1, 1}); });
        for (PictureDrawGroup invalid : {PictureDrawGroup{0, 2}, PictureDrawGroup{2, 0},
              PictureDrawGroup{std::numeric_limits<std::size_t>::max(), 1}}) {
            const std::array<PictureDrawGroup, 1> groups{invalid};
            rejects([&] { (void)compute_picture_bounds(data, groups, {1, 1}); });
        }
        const std::array<PictureDrawGroup, 1> zero{{{0, 1}}};
        rejects([&] { (void)compute_picture_bounds(data, zero, {1, 1}); });
        for (float invalid : {0.0F, -0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
            rejects([&] { (void)compute_picture_bounds(data, one, {invalid, 1}); });
            rejects([&] { (void)compute_picture_bounds(data, one, {1, invalid}); });
        }
        for (auto member : {&PictureResourceDescriptor::local_center_x,
                            &PictureResourceDescriptor::local_center_y,
                            &PictureResourceDescriptor::horizontal_edge_span,
                            &PictureResourceDescriptor::vertical_edge_span}) {
            for (float invalid : {std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()}) {
                auto bad = data; bad[0].*member = invalid;
                rejects([&] { (void)compute_picture_bounds(bad, one, {1, 1}); });
            }
        }
        for (auto member : {&PictureResourceDescriptor::horizontal_edge_span,
                            &PictureResourceDescriptor::vertical_edge_span}) {
            auto bad = data; bad[0].*member = -1;
            rejects([&] { (void)compute_picture_bounds(bad, one, {1, 1}); });
            bad[0].*member = -0.0F;
            (void)compute_picture_bounds(bad, one, {1, 1});
        }
        for (int mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
            if (std::fesetround(mode) == 0)
                rejects([&] { (void)compute_picture_bounds(data, one, {1, 1}); });
        }
        check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding");
        data[0] = descriptor(std::numeric_limits<float>::max(), 0, std::numeric_limits<float>::max(), 1);
        rejects([&] { (void)compute_picture_bounds(data, one, {1, 1}); });
        data[0] = descriptor(1.0e20F, 0, 1.0e20F, 1);
        rejects([&] { (void)compute_picture_bounds(data, one, {1, 1}); });
        data[0] = descriptor(3, 0, 2, 1);
        rejects([&] { (void)compute_picture_bounds(data, one, {std::numeric_limits<float>::max(), 1}); });
    }
    check(std::fesetround(saved_rounding) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
