#include "off/graphics/fade_picture_size.hpp"

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
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported initialization must throw");
}
PictureCacheTransformInput transform_input() {
    constexpr std::array<float, 9> basis{0, 0, 1, 0, 1, 0, 1, 0, 0};
    return {.submission_position = {0, 0, 0},
            .aligned_local_position = {50, 40, 0},
            .virtual_window_scale = {7, 9, 1, 1},
            .cached_basis = basis, .object_matrix = basis,
            .viewport_width = 100, .viewport_height = 80,
            .picture_width = 1, .picture_height = 1,
            .owner_projection_scalar = 2, .external_y_basis_scale = 2};
}

void check_borrowed_scale_initialization() {
    FadePictureSize size;
    PictureSubmissionCache cache;
    std::array<float, 2> scale{3, 4};
    const std::array<off::data::BoundPictureDrawGroup, 0> groups{};
    auto input = transform_input();
    const auto prepare = [&] {
        input.picture_width = scale[0];
        input.picture_height = scale[1];
        cache.submit(groups, input, 0, {});
    };
    prepare();
    const auto previous = cache.cached_state()->transform;
    int notifications = 0;
    size.initialize(scale, 640, 480, cache, [&] {
        ++notifications;
        check(scale == std::array<float, 2>{41, 31} && cache.dirty(),
              "borrowed hook sees both canonical scales committed before notification");
        check(cache.cached_state()->transform.basis == previous.basis &&
                  size.scale() == std::array<float, 2>{1, 1},
              "borrowed initialization leaves cached transform and standalone storage untouched");
    });
    prepare();
    check(cache.cached_state()->transform.basis == prepare_picture_cache_transform(input).basis &&
              cache.cached_state()->transform.basis != previous.basis,
          "borrowed canonical scales feed the invalidated transform");
    size.initialize(scale, 655, 495, cache, {});
    check(notifications == 1 && !cache.dirty(),
          "equal borrowed scales need no notification hook or invalidation");

    const auto retained = scale;
    const auto notify = [&] { ++notifications; };
    for (const auto invalid : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        rejects([&] { size.initialize(scale, invalid, 1, cache, notify); });
        rejects([&] { size.initialize(scale, 1, invalid, cache, notify); });
    }
    rejects([&] { size.initialize(scale, 16, 16, cache, {}); });
    check(scale == retained && !cache.dirty() && notifications == 1,
          "invalid borrowed inputs preserve canonical scale and cache");
    for (const auto mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0) {
            rejects([&] { size.initialize(scale, 16, 16, cache, notify); });
            check(scale == retained && !cache.dirty() && notifications == 1,
                  "unsupported borrowed rounding fails before owner mutations");
        }
    }
    check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding for borrowed state");

    rejects([&] { size.initialize(scale, 64, 80, cache, [&] {
        ++notifications;
        check(scale == std::array<float, 2>{5, 6} && cache.dirty(),
              "failing borrowed hook observes the committed canonical prefix");
        throw std::runtime_error("borrowed resource notification failure");
    }); });
    check(scale == std::array<float, 2>{5, 6} && cache.dirty(),
          "borrowed notification failure retains scale and cache mutations");
    prepare();
    size.initialize(scale, 64, 80, cache, {});
    check(notifications == 2 && !cache.dirty(),
          "equal borrowed retry does not repeat a failed notification");

    size.initialize(scale, 80, 96, cache, [&] {
        rejects([&] { size.initialize(scale, 80, 96, cache, {}); });
        rejects([&] { size.initialize(scale, 96, 112, cache, notify); });
        rejects([&] { size.initialize(1, 1, cache, {}); });
        rejects([&] { size.initialize(16, 16, cache, notify); });
        check(scale == std::array<float, 2>{6, 7} &&
                  size.scale() == std::array<float, 2>{1, 1},
              "borrowed callback rejects equal and changed reentry through both overloads");
    });
    size.initialize(16, 16, cache, [&] {
        rejects([&] { size.initialize(scale, 80, 96, cache, {}); });
        rejects([&] { size.initialize(scale, 96, 112, cache, notify); });
        check(scale == std::array<float, 2>{6, 7} &&
                  size.scale() == std::array<float, 2>{2, 2},
              "standalone callback uses the same guard as borrowed initialization");
    });
    rejects([&] { size.initialize(scale, 96, 112, cache, [&] {
        size.initialize(32, 32, cache, notify);
    }); });
    check(scale == std::array<float, 2>{7, 8} &&
              size.scale() == std::array<float, 2>{2, 2},
          "uncaught cross-overload reentry retains only the outer canonical write");
    size.initialize(scale, 112, 128, cache, notify);
    check(scale == std::array<float, 2>{8, 9},
          "shared guard releases after a borrowed callback throws");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<FadePictureSize> &&
                  !std::is_move_constructible_v<FadePictureSize> &&
                  !std::is_copy_assignable_v<FadePictureSize> &&
                  !std::is_move_assignable_v<FadePictureSize>);
    const int original_rounding = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) {
        std::cerr << "FAIL: nearest rounding unavailable\n";
        return 1;
    }
    FadePictureSize size;
    PictureSubmissionCache cache;
    const std::array<off::data::BoundPictureDrawGroup, 0> groups{};
    const auto table = std::span<const off::data::BoundPictureDrawGroup>(groups);
    auto input = transform_input();
    const auto prepare = [&] {
        input.picture_width = size.scale()[0]; input.picture_height = size.scale()[1];
        cache.submit(table, input, 0, {});
    };
    check(size.scale() == std::array<float, 2>{1, 1} && !cache.cached_state(),
          "constructor extents do not manufacture a cached transform");
    prepare();
    const auto previous = cache.cached_state()->transform;
    size.initialize(15, 1, cache, {});
    check(!cache.dirty(), "same initial extents need no hook or invalidation");
    int notifications = 0;
    size.initialize(640, 480, cache, [&] {
        ++notifications;
        check(size.scale() == std::array<float, 2>{41, 31} && cache.dirty(),
              "hook sees both committed dimensions and dirty state");
        check(cache.cached_state()->transform.basis == previous.basis,
              "invalidation retains previous transform until successful preparation");
    });
    prepare();
    const auto expected = prepare_picture_cache_transform(input);
    check(!cache.dirty() && cache.cached_state()->transform.basis == expected.basis &&
              cache.cached_state()->transform.translation == expected.translation &&
              cache.cached_state()->transform.basis != previous.basis,
          "unchanged-position submission recomputes from new explicit picture scale");
    size.initialize(655, 495, cache, [&] { ++notifications; });
    check(notifications == 1 && !cache.dirty(), "equal integer quotient pair is a complete setter no-op");
    size.initialize(656, 495, cache, [&] { ++notifications; });
    check(size.scale() == std::array<float, 2>{42, 31} && notifications == 2 && cache.dirty(),
          "one-axis change invalidates");
    prepare();
    const auto retained = size.scale();
    for (const auto invalid : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        rejects([&] { size.initialize(invalid, 1, cache, [&] { ++notifications; }); });
        rejects([&] { size.initialize(1, invalid, cache, [&] { ++notifications; }); });
    }
    rejects([&] { size.initialize(16, 16, cache, {}); });
    check(size.scale() == retained && !cache.dirty() && notifications == 2,
          "invalid dimensions and missing required hook fail before mutation");
    for (const auto mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0) {
            rejects([&] { size.initialize(16, 16, cache, [&] { ++notifications; }); });
            check(size.scale() == retained && !cache.dirty(), "unsupported rounding preserves owner and cache");
        }
    }
    check(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding for conversion tests");
    const auto notify = [&] { ++notifications; };
    size.initialize(16, 31, cache, notify);
    check(size.scale() == std::array<float, 2>{2, 2}, "signed integer division precedes addition and conversion");
    size.initialize(32, 15, cache, notify);
    check(size.scale() == std::array<float, 2>{3, 1}, "dimension multiple and preceding bucket boundaries");
    size.initialize(268435440, 1, cache, notify);
    check(size.scale()[0] == 16777216.0F, "large exactly representable scale");
    prepare();
    const auto before_rounding_collision = notifications;
    size.initialize(268435456, 1, cache, notify);
    check(size.scale()[0] == 16777216.0F && notifications == before_rounding_collision && !cache.dirty(),
          "integer-to-float ties-to-even collision does not spuriously invalidate");
    size.initialize(268435488, 1, cache, notify);
    check(size.scale()[0] == 16777220.0F, "upper ties-to-even conversion");
    size.initialize(std::numeric_limits<std::int32_t>::max(), 1, cache, notify);
    check(size.scale()[0] == 134217728.0F, "maximum signed input division and increment do not overflow");

    rejects([&] { size.initialize(48, 64, cache, [&] {
        check(size.scale() == std::array<float, 2>{4, 5} && cache.dirty(),
              "throwing hook observes committed dimensions and invalidation");
        throw std::runtime_error("resource notification failure");
    }); });
    check(size.scale() == std::array<float, 2>{4, 5} && cache.dirty(), "hook exception does not roll back writes");
    size.initialize(48, 64, cache, {});
    prepare();
    size.initialize(64, 80, cache, [&] {
        rejects([&] { size.initialize(64, 80, cache, {}); });
        rejects([&] { size.initialize(80, 96, cache, notify); });
        check(size.scale() == std::array<float, 2>{5, 6}, "nested calls cannot mutate committed outer scale");
    });
    rejects([&] { size.initialize(80, 96, cache, [&] { size.initialize(1, 1, cache, notify); }); });
    check(size.scale() == std::array<float, 2>{6, 7}, "uncaught nested rejection retains outer committed write");
    size.initialize(96, 112, cache, notify);
    check(size.scale() == std::array<float, 2>{7, 8}, "guard releases after callback exception");
    check_borrowed_scale_initialization();
    if (original_rounding != -1) check(std::fesetround(original_rounding) == 0, "restore incoming rounding mode");
    return failures == 0 ? 0 : 1;
}
