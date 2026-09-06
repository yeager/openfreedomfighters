#include "off/graphics/picture_bounds.hpp"
#include "off/graphics/picture_submission_cache.hpp"

#include <array>
#include <bit>
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
    {
        PictureBoundsApplication app;
        ResourceBounds bounds{{11, 12, 13}, {14, 15, 16}, 17};
        std::uint32_t flags = 0x400U;
        std::array<float, 2> alignment{77, 88};
        PictureSubmissionCache cache;
        const PictureCacheTransformInput input{
            .virtual_window_scale = {0, 0, 1, 1},
            .cached_basis = {0, 0, 1, 0, 1, 0, 1, 0, 0},
            .object_matrix = {0, 0, 1, 0, 1, 0, 1, 0, 0},
            .viewport_width = 100, .viewport_height = 100,
            .picture_width = 1, .picture_height = 1,
            .owner_projection_scalar = 1, .external_y_basis_scale = 1};
        const auto clean = [&] {
            cache.submit(std::span<const off::data::BoundPictureDrawGroup>{}, input, 0, {});
            check(!cache.dirty(), "prepare a clean cache before materialization");
        };
        clean();
        int queries = 0;
        const auto query = [&](std::uint64_t, std::uint64_t, auto&, auto&) {
            ++queries;
            check(!cache.dirty() && alignment == std::array<float, 2>{77, 88},
                  "alignment and cache writes follow renderer query and bounds writes");
            return false;
        };
        rejects([&] { app.apply_materialized(bounds, flags, 41, 0, descriptors, groups,
                                            {1, 1}, query, 16, alignment, cache); });
        check(queries == 0 && !cache.dirty() && bounds.radius == 17 && !app.failed(),
              "invalid alignment rejects before query or state writes");
        app.apply_materialized(bounds, flags, 41, 0, descriptors, groups,
                               {1, 1}, query, 0, alignment, cache);
        check(final_bounds(bounds) && alignment == std::array<float, 2>{0, 8} && cache.dirty(),
              "complete callback aligns from own signed center and clamped extents then invalidates cache");
        clean();
        app.apply_materialized(bounds, flags, 41, 0, descriptors, groups,
                               {2, 3}, query_zero_renderer_resource_bounds, 8, alignment, cache);
        check(alignment == std::array<float, 2>{-6, 12} && cache.dirty(),
              "repeat materialization uses current scale and center-only alignment");
        clean();
        const auto saved_alignment = alignment;
        rejects([&] { app.apply_materialized(bounds, flags, 41, 0, descriptors, groups,
            {1, 1}, [](std::uint64_t, std::uint64_t, auto&, auto&) -> bool {
                throw std::runtime_error("query failed before alignment");
            }, 0, alignment, cache); });
        check(app.failed() && alignment == saved_alignment && !cache.dirty(),
              "failed query leaves alignment/cache tail unapplied and poisons callback");
    }
    {
        std::array<float, 3> center{-0.0F, std::numeric_limits<float>::quiet_NaN(), 17};
        std::array<float, 3> extents{std::numeric_limits<float>::infinity(), -0.0F, -5};
        const auto center_bits = std::bit_cast<std::array<std::uint32_t, 3>>(center);
        const auto extent_bits = std::bit_cast<std::array<std::uint32_t, 3>>(extents);
        for (std::uint64_t identity : {std::uint64_t{0}, std::uint64_t{41},
                                      std::numeric_limits<std::uint64_t>::max()}) {
            check(!query_zero_renderer_resource_bounds(identity, 0, center, extents),
                  "guarded zero renderer identifier returns false independently of opaque owner identity");
            check(std::bit_cast<std::array<std::uint32_t, 3>>(center) == center_bits &&
                  std::bit_cast<std::array<std::uint32_t, 3>>(extents) == extent_bits,
                  "zero renderer query neither validates nor changes output bits");
        }
        for (std::uint64_t identifier : {std::uint64_t{1}, std::numeric_limits<std::uint64_t>::max()}) {
            rejects([&] { (void)query_zero_renderer_resource_bounds(41, identifier, center, extents); });
            check(std::bit_cast<std::array<std::uint32_t, 3>>(center) == center_bits &&
                  std::bit_cast<std::array<std::uint32_t, 3>>(extents) == extent_bits,
                  "unsupported renderer identifier rejects before output mutation");
        }
        ResourceBounds bounds{center, extents, -3};
        std::uint32_t flags = 0x400U;
        PictureBoundsApplication app;
        app.apply(bounds, flags, 41, 0, descriptors, groups, {1, 1}, query_zero_renderer_resource_bounds);
        check(final_bounds(bounds) && flags == 0x100400U && !app.failed(),
              "concrete zero-query failure is followed by complete materialized descriptor application");
    }
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
