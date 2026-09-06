#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/picture_view_parameters.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using off::data::GmsIntroCameraSource;
using off::graphics::convert_intro_camera_mode_zero;
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported camera conversion must reject");
}
GmsIntroCameraSource source() {
    GmsIntroCameraSource value{};
    value.near_distance = 0.375;
    value.far_distance = 250.125;
    value.auxiliary_scalar = 0.1234567890123;
    value.angle_degrees = 5;
    value.background_rgb = {0x123456abU, 0xffffffcdU, 0x800000efU};
    value.integer_a = 0xdeadbeefU;
    value.priority = 0xffffffffU;
    value.flag_option_a = 0x87654321U;
    value.flag_option_b = 0xfedcba98U;
    value.auxiliary_floats = {-3.25F, 7.5F};
    value.viewport = {-0.0F, -0.0F, 0.5F, 0.25F};
    value.final_boolean = 0x80000000U;
    return value;
}
}

int main() {
    const auto saved_rounding = std::fegetround();
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    auto authored = source();
    const auto camera = convert_intro_camera_mode_zero(authored);
    check(camera.near_distance == 1.0F && camera.far_distance == 250.125F &&
          camera.auxiliary_scalar == static_cast<float>(authored.auxiliary_scalar),
          "reader narrowing and near-one clamp are separate from projection");
    check(std::bit_cast<std::uint32_t>(camera.angle_radians) == 0x3db2b8c3U,
          "angle multiply then divide retains each binary32 rounding boundary");
    check(camera.registration_priority == -1.0F && camera.background == 0x00abcdefU && camera.final_boolean,
          "priority signed interpretation, background low bytes and boolean nonzero conversion");
    check(camera.fog_start_fraction == -3.25F && camera.fog_end_fraction == 7.5F,
          "fog fractions are copied without clamping or distance scaling");
    auto fog_source = source();
    fog_source.auxiliary_floats = {-0.0F, std::numeric_limits<float>::denorm_min()};
    const auto fog_camera = convert_intro_camera_mode_zero(fog_source);
    check(std::bit_cast<std::uint32_t>(fog_camera.fog_start_fraction) == 0x80000000U &&
          std::bit_cast<std::uint32_t>(fog_camera.fog_end_fraction) == 1U,
          "fog source preserves signed zero and binary32 payload without arithmetic");
    check(camera.authored && camera.authored->auxiliary_scalar == 0.1234567890123 && camera.authored->angle_degrees == 5 &&
          camera.authored->integer_a == 0xdeadbeefU && camera.authored->priority == 0xffffffffU &&
          camera.authored->flag_option_a == 0x87654321U && camera.authored->flag_option_b == 0xfedcba98U &&
          camera.authored->auxiliary_floats == authored.auxiliary_floats,
          "authored precision and opaque options are preserved independently");
    check(std::bit_cast<std::uint32_t>(camera.viewport[0]) == 0U &&
          std::bit_cast<std::uint32_t>(camera.viewport[1]) == 0U &&
          std::bit_cast<std::uint32_t>(camera.authored->viewport[0]) == 0x80000000U &&
          camera.viewport_ratio == 0.5F, "identity composition is not a bitwise viewport copy");
    authored.priority = 0x80000000U;
    authored.far_distance = -0.0; authored.auxiliary_scalar = -0.0; authored.angle_degrees = -0.0;
    authored.final_boolean = 0;
    const auto zero = convert_intro_camera_mode_zero(authored);
    check(zero.registration_priority == -2147483648.0F && !zero.final_boolean &&
          std::bit_cast<std::uint32_t>(zero.far_distance) == 0x80000000U &&
          std::bit_cast<std::uint32_t>(zero.auxiliary_scalar) == 0x80000000U &&
          std::bit_cast<std::uint32_t>(zero.angle_radians) == 0x80000000U,
          "signed-zero conversions and minimum signed priority");
    authored = source(); authored.priority = 0x7fffffffU; authored.near_distance = 2.125;
    authored.far_distance = -2;
    check(convert_intro_camera_mode_zero(authored).registration_priority == 2147483648.0F &&
          convert_intro_camera_mode_zero(authored).near_distance == 2.125F &&
          convert_intro_camera_mode_zero(authored).far_distance == -2,
          "raw reader does not repair far ordering and priority rounds to binary32");
    const auto reject_mutation = [&](auto mutate) {
        auto input = source(); mutate(input);
        rejects([&] { static_cast<void>(convert_intro_camera_mode_zero(input)); });
    };
    reject_mutation([](auto& s) { s.aspect_mode = 1; });
    reject_mutation([](auto& s) { s.renderer_list_selector = 1; });
    for (auto member : {&GmsIntroCameraSource::near_distance, &GmsIntroCameraSource::far_distance,
                        &GmsIntroCameraSource::auxiliary_scalar, &GmsIntroCameraSource::angle_degrees}) {
        for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               2.0 * std::numeric_limits<float>::max()})
            reject_mutation([&](auto& s) { s.*member = invalid; });
    }
    reject_mutation([](auto& s) { s.angle_degrees = std::numeric_limits<float>::max(); });
    for (std::size_t i = 0; i < 2; ++i)
        reject_mutation([&](auto& s) { s.auxiliary_floats[i] = std::numeric_limits<float>::quiet_NaN(); });
    for (std::size_t i = 0; i < 4; ++i)
        reject_mutation([&](auto& s) { s.viewport[i] = std::numeric_limits<float>::infinity(); });
    for (std::size_t i = 2; i < 4; ++i) {
        for (float invalid : {0.0F, -0.0F, -1.0F})
            reject_mutation([&](auto& s) { s.viewport[i] = invalid; });
    }
    reject_mutation([](auto& s) {
        s.viewport[2] = std::numeric_limits<float>::min();
        s.viewport[3] = std::numeric_limits<float>::max();
    });
    authored = source(); authored.viewport = {-2, 3, 4, 2};
    const auto relative = convert_intro_camera_mode_zero(authored);
    check(relative.viewport == authored.viewport && relative.viewport_ratio == 0.5F,
          "finite offsets and oversized positive extents are not silently clamped");
    for (const auto mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0)
            rejects([&] { static_cast<void>(convert_intro_camera_mode_zero(source())); });
    }
    check(std::fesetround(FE_TONEAREST) == 0, "restore supported conversion environment");

    // Explicit integration inputs: these are not recovered intro pass dimensions.
    const off::graphics::PictureViewCommonInput common{
        .raw_near = camera.near_distance, .selected_far = camera.far_distance,
        .rectangle = {10, 20, 210, 120}, .renderer_dimension_0 = 200,
        .renderer_dimension_1 = 100, .virtual_aspect = 1};
    const auto view = off::graphics::prepare_picture_view_parameters(common,
        off::graphics::PictureOrdinaryCameraInput{camera.angle_radians, 1});
    check(camera.near_distance == 1 && view.near_distance == 5 && view.far_distance == camera.far_distance &&
          view.half_extent_0 > 0 && view.half_extent_1 > 0,
          "existing view helper consumes converted radians and applies independent near-five clamp");
    const auto viewport = off::graphics::prepare_picture_viewport_request(common.rectangle, camera.viewport);
    check(viewport == std::array<float, 4>{10, 20, 100, 25},
          "composed relative viewport maps into explicitly supplied pass rectangle");
    check(camera.authored->near_distance == 0.375 && camera.authored->viewport[2] == 0.5F,
          "downstream preparation does not mutate authored source");
    if (saved_rounding != -1) check(std::fesetround(saved_rounding) == 0, "restore incoming rounding environment");
    return failures == 0 ? 0 : 1;
}
