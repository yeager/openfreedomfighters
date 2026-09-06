#include "off/graphics/fresh_intro_camera.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using off::graphics::FreshIntroCamera;
int failures = 0;
void check(bool condition, const char* text) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
template<class F> void rejects(F operation) {
    bool caught = false;
    try { operation(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, "unsupported fresh camera operation rejects");
}
off::data::GmsIntroCameraSource source() {
    off::data::GmsIntroCameraSource s{};
    s.near_distance = 0.375; s.far_distance = 321.25;
    s.auxiliary_scalar = 0.1234567890123; s.angle_degrees = 5;
    s.background_rgb = {0x123456abU, 0xffffffcdU, 0x800000efU};
    s.integer_a = 0xdeadbeefU; s.priority = 0xffffffffU;
    s.auxiliary_floats = {-3.25F, 7.5F};
    s.viewport = {-0.0F, 0, 0.5F, 0.25F};
    return s;
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<FreshIntroCamera> &&
                  !std::is_move_constructible_v<FreshIntroCamera> &&
                  !std::is_copy_assignable_v<FreshIntroCamera> &&
                  !std::is_move_assignable_v<FreshIntroCamera>);
    const int saved = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    for (std::uint32_t a : {0U, 1U, 7U, 0xffffffffU})
        for (std::uint32_t b : {0U, 1U, 9U, 0x80000000U})
            for (std::uint32_t final : {0U, 3U}) {
                auto s = source(); s.flag_option_a = a; s.flag_option_b = b; s.final_boolean = final;
                FreshIntroCamera camera(s);
                const auto expected = 0x20U | (a == 0 ? 0x80000U : 0U) | (b != 0 ? 0x10000U : 0U);
                check(camera.flags() == expected && camera.enabled() && camera.render_control() == 0 &&
                      !camera.associated_target() && camera.parameters().final_boolean == (final != 0),
                      "fresh flag word uses inverse A/direct B truth, independent final boolean and null target");
                check(camera.parameters().authored.flag_option_a == a && camera.parameters().authored.flag_option_b == b,
                      "raw option words remain owned beside converted runtime flags");
            }
    {
        auto s = source();
        s.auxiliary_floats = {-0.0F, 0.375F};
        FreshIntroCamera camera(s);
        s.near_distance = 999; s.viewport[2] = 0; s.flag_option_a = 19;
        s.auxiliary_floats = {42, 99};
        const auto& p = camera.parameters();
        check(std::bit_cast<std::uint32_t>(p.fog_start_fraction) == 0x80000000U &&
              p.fog_end_fraction == 0.375F,
              "canonical camera owns fog fractions independently of source lifetime");
        check(p.authored.near_distance == 0.375 && p.near_distance == 1 && p.far_distance == 321.25 &&
              p.authored.auxiliary_scalar == 0.1234567890123 &&
              std::bit_cast<std::uint32_t>(p.angle_radians) == 0x3db2b8c3U &&
              p.registration_priority == -1 && p.background == 0x00abcdefU && p.viewport_ratio == 0.5F,
              "instance owns source precision and existing rounded conversion results");
        check(std::bit_cast<std::uint32_t>(p.authored.viewport[0]) == 0x80000000U &&
              std::bit_cast<std::uint32_t>(p.viewport[0]) == 0,
              "authored signed zero stays distinct from composed viewport");
        const auto original = camera.flags();
        int notices = 0;
        camera.set_enabled(true, true, {});
        rejects([&] { camera.set_enabled(false, true, {}); });
        check(camera.flags() == original, "idempotence needs no hook; changed missing hook retains flags");
        camera.set_enabled(false, true, [&] {
            ++notices;
            check(camera.flags() == original && camera.enabled(), "renderer observes canonical enabled bit before store");
            rejects([&] { camera.set_enabled(true, false, {}); });
        });
        check(!camera.enabled() && camera.flags() == (original & ~0x20U) && notices == 1,
              "enable transition mutates single canonical word while retaining source-derived bits");
        check(std::bit_cast<std::uint32_t>(p.fog_start_fraction) == 0x80000000U &&
              p.fog_end_fraction == 0.375F,
              "enabled transition does not rewrite current fog fractions");
        rejects([&] { camera.set_enabled(true, true, [&] {
            ++notices; check(!camera.enabled(), "throwing hook sees pre-transition state");
            throw std::runtime_error("renderer failure");
        }); });
        check(!camera.enabled() && camera.flags() == (original & ~0x20U), "throwing renderer leaves canonical bit unchanged");
        camera.set_enabled(true, false, [&] { ++notices; });
        check(camera.flags() == original && notices == 2 && camera.render_control() == 0 && !camera.associated_target(),
              "absent renderer permits later direct transition without touching unrelated runtime fields");
    }
    const auto invalid = [&](auto mutate) {
        auto s = source(); mutate(s);
        rejects([&] { FreshIntroCamera camera(s); });
    };
    invalid([](auto& s) { s.aspect_mode = 1; });
    invalid([](auto& s) { s.renderer_list_selector = 1; });
    invalid([](auto& s) { s.viewport[2] = 0; });
    invalid([](auto& s) { s.viewport[3] = -1; });
    invalid([](auto& s) { s.near_distance = std::numeric_limits<double>::infinity(); });
    invalid([](auto& s) { s.far_distance = std::numeric_limits<double>::max(); });
    invalid([](auto& s) { s.auxiliary_scalar = std::numeric_limits<double>::quiet_NaN(); });
    invalid([](auto& s) { s.angle_degrees = std::numeric_limits<double>::quiet_NaN(); });
    invalid([](auto& s) { s.auxiliary_floats[1] = std::numeric_limits<float>::infinity(); });
    invalid([](auto& s) { s.viewport = {0, 0, std::numeric_limits<float>::min(), std::numeric_limits<float>::max()}; });
    for (int mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        if (std::fesetround(mode) == 0) invalid([](auto&) {});
    check(std::fesetround(saved) == 0, "restore caller rounding mode");
    return failures == 0 ? 0 : 1;
}
