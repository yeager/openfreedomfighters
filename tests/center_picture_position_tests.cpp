#include "off/graphics/center_picture_position.hpp"
#include "off/cutscene/picture_activation_prefix.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool value, const char* text) {
    if (!value) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported operation throws");
}
PictureCacheTransformInput input() {
    constexpr std::array<float, 9> basis{0, 0, 1, 0, 1, 0, 1, 0, 0};
    return {.submission_position = {0, 0, 0}, .aligned_local_position = {50, 40, 0},
            .virtual_window_scale = {7, 9, 1, 1}, .cached_basis = basis,
            .object_matrix = basis, .viewport_width = 100, .viewport_height = 80,
            .picture_width = 1, .picture_height = 1,
            .owner_projection_scalar = 2, .external_y_basis_scale = 2};
}
void prepare(PictureSubmissionCache& cache) {
    const std::array<off::data::BoundPictureDrawGroup, 0> groups{};
    cache.submit(std::span<const off::data::BoundPictureDrawGroup>(groups), input(), 0, {});
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<CenterPicturePosition> &&
                  !std::is_move_constructible_v<CenterPicturePosition> &&
                  !std::is_copy_assignable_v<CenterPicturePosition> &&
                  !std::is_move_assignable_v<CenterPicturePosition>);
    const int saved = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    CenterPicturePosition center;
    PictureSubmissionCache cache;
    prepare(cache);
    std::array<float, 3> position{9, -3, 4};
    std::uint32_t flags = 0x400U, status = 0x80U;
    int calls = 0;
    const auto previous = cache.cached_state()->transform;
    center.initialize(position, flags, status, 101, 83, cache, [&] {
        ++calls;
        check(position == std::array<float, 3>{50, 41, 0}, "integer halves precede float conversion");
        check(flags == 0x100400U && status == 0x80U, "service sees new flags but old status");
        check(!cache.dirty() && cache.cached_state()->transform.translation == previous.translation,
              "service precedes cache invalidation");
    });
    check(calls == 1 && cache.dirty() && status == 0x81U, "cache and retained status updated after service");
    prepare(cache);
    position[2] = -0.0F;
    flags = 0x20U; status = 0x40U;
    center.initialize(position, flags, status, 101, 83, cache, {});
    check(std::bit_cast<std::uint32_t>(position[2]) == 0x80000000U && flags == 0x20U &&
          status == 0x41U && !cache.dirty(), "numeric equality preserves signed zero and only marks status");
    const auto retained = position;
    for (auto invalid : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        rejects([&] { center.initialize(position, flags, status, invalid, 83, cache, [&] { ++calls; }); });
        rejects([&] { center.initialize(position, flags, status, 101, invalid, cache, [&] { ++calls; }); });
    }
    rejects([&] { center.initialize(position, flags, status, 200, 83, cache, {}); });
    rejects([&] { center.initialize(position, flags, flags, 101, 83, cache, {}); });
    for (int mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0)
            rejects([&] { center.initialize(position, flags, status, 101, 83, cache, {}); });
    }
    check(std::fesetround(FE_TONEAREST) == 0, "restore rounding");
    check(position == retained && flags == 0x20U && status == 0x41U && calls == 1 && !cache.dirty(),
          "invalid scalar inputs have no effects even on otherwise equal path");
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (float invalid : {std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
            auto bad = retained; bad[axis] = invalid;
            const auto bits = std::bit_cast<std::array<std::uint32_t, 3>>(bad);
            rejects([&] { center.initialize(bad, flags, status, 101, 83, cache, [&] { ++calls; }); });
            check(std::bit_cast<std::array<std::uint32_t, 3>>(bad) == bits && flags == 0x20U &&
                  status == 0x41U && !cache.dirty() && calls == 1, "nonfinite rejection precedes all effects");
        }
    }
    center.initialize(position, flags, status, 33554434, 33554438, cache, [] {});
    check(position == std::array<float, 3>{16777216, 16777220, 0}, "large integer halves round to even binary32");
    center.initialize(position, flags, status, std::numeric_limits<std::int32_t>::max(), 1, cache, [] {});
    check(position == std::array<float, 3>{1073741824, 0, 0}, "maximum signed extent and smallest positive extent");
    prepare(cache); status = 0x10U; flags = 0x800U;
    rejects([&] { center.initialize(position, flags, status, 21, 19, cache, [&] {
        check(position == std::array<float, 3>{10, 9, 0} && flags == 0x100800U,
              "throwing service sees committed prefix");
        throw std::runtime_error("test service failure");
    }); });
    check(!cache.dirty() && status == 0x10U && flags == 0x100800U &&
          position == std::array<float, 3>{10, 9, 0}, "exception does not force suffix or roll back prefix");
    center.initialize(position, flags, status, 21, 19, cache, {});
    check(status == 0x11U && !cache.dirty(), "guard released after throw and equal retry needs no hook");
    center.initialize(position, flags, status, 23, 19, cache, [&] {
        rejects([&] { center.initialize(position, flags, status, 23, 19, cache, {}); });
        check(!cache.dirty(), "reentrant equal call cannot write cache or bypass guard");
    });
    check(cache.dirty(), "outer operation completes after rejected reentry");

    // Explicit phase-one admission; this is not a complete scene lifecycle.
    prepare(cache); position = {2, 3, 4}; flags = 0x400U; status = 0x20U;
    off::cutscene::PictureActivationPrefix activation;
    using Stage = off::cutscene::PictureActivationPrefix::Stage;
    std::vector<Stage> stages;
    activation.run(flags, 0x400U, true, [&](Stage stage) {
        stages.push_back(stage);
        if (stage == Stage::phase_one)
            center.initialize(position, flags, status, 321, 241, cache, [&] {
                check(flags == 0x100400U && !cache.dirty() && status == 0x20U,
                      "blocked parent still admits outer phase-one Center effects");
            });
    });
    check(stages == std::vector<Stage>{Stage::append_tracking, Stage::parent_blocked,
          Stage::phase_one, Stage::record_requested} && flags == 0x100400U && status == 0x21U &&
          position == std::array<float, 3>{160, 120, 0} && cache.dirty(),
          "Center does not clear blocked authored-derived hidden state or omit requested suffix");
    check(std::fesetround(saved) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
