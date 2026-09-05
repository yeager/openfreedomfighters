#include "off/data/picture_texture_binding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::size_t index_bytes = 2048U * 4U;
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}
void set32(std::vector<std::byte>& out, std::size_t at, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out[at++] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}
std::vector<std::byte> catalog_bytes(
    std::uint32_t image_id = 7,
    bool sequence = false
) {
    std::vector<std::byte> bytes(16, std::byte{0});
    const auto image = bytes.size();
    u32(bytes, 46); u32(bytes, 0x52474241); u32(bytes, 0x52474241);
    u32(bytes, image_id); u32(bytes, 0x00010001); u32(bytes, 1);
    u32(bytes, 0); u32(bytes, 0); u32(bytes, 0);
    bytes.push_back(std::byte{'x'}); bytes.push_back(std::byte{0});
    u32(bytes, 4); u32(bytes, 0xff112233);
    std::size_t sequence_offset = 0;
    if (sequence) {
        sequence_offset = bytes.size();
        u32(bytes, 1);
        u32(bytes, image_id);
    }
    const auto data_end = bytes.size();
    bytes.resize(data_end + index_bytes * 2, std::byte{0});
    set32(bytes, data_end + image_id * 4, static_cast<std::uint32_t>(image));
    if (sequence) {
        set32(bytes, data_end + index_bytes + image_id * 4,
              static_cast<std::uint32_t>(sequence_offset));
    }
    set32(bytes, 0, static_cast<std::uint32_t>(data_end));
    set32(bytes, 4, static_cast<std::uint32_t>(data_end + index_bytes));
    set32(bytes, 8, 3); set32(bytes, 12, 4);
    return bytes;
}
off::data::PictureTextureResource resource(std::uint16_t key) {
    return {.prm_offset = 32, .manager_key = key};
}
template<class F> bool rejects(F&& call) {
    try { call(); return false; } catch (const std::runtime_error&) { return true; }
}
}

int main() {
    const auto bytes = catalog_bytes();
    const auto catalog = off::data::TextureCatalog::parse(bytes);
    const std::array direct{resource(7)};
    const auto direct_join = off::data::PictureTextureBindings::build(direct, catalog);
    check(direct_join.entries()[0].prm_offset == 32 &&
          direct_join.entries()[0].manager_key == 7 &&
          direct_join.entries()[0].texture_id == 7 &&
          direct_join.entries()[0].image_index == 0 &&
          direct_join.entries()[0].bank == off::data::TextureManagerKeyBank::direct,
          "preserve a complete direct-bank binding");
    const std::array upper{resource(2055), resource(2055)};
    const auto upper_join = off::data::PictureTextureBindings::build(upper, catalog, true);
    check(upper_join.entries().size() == 2 &&
          upper_join.entries()[0].prm_offset == 32 &&
          upper_join.entries()[0].manager_key == 2055 &&
          upper_join.entries()[0].texture_id == 7 &&
          upper_join.entries()[0].image_index == 0,
          "preserve a complete repeated upper-bank binding");

    const auto zero_bytes = catalog_bytes(0);
    const auto zero_catalog = off::data::TextureCatalog::parse(zero_bytes);
    for (const auto key : std::array<std::uint16_t, 2>{0, 2048}) {
        const std::array values{resource(key)};
        const auto joined = off::data::PictureTextureBindings::build(values, zero_catalog);
        check(joined.entries()[0].manager_key == key &&
              joined.entries()[0].texture_id == 0 &&
              joined.entries()[0].image_index == 0,
              "resolve the lower texture-slot boundary in both banks");
    }
    const auto last_bytes = catalog_bytes(2047);
    const auto last_catalog = off::data::TextureCatalog::parse(last_bytes);
    for (const auto key : std::array<std::uint16_t, 2>{2047, 4095}) {
        const std::array values{resource(key)};
        const auto joined = off::data::PictureTextureBindings::build(values, last_catalog);
        check(joined.entries()[0].manager_key == key &&
              joined.entries()[0].texture_id == 2047 &&
              joined.entries()[0].image_index == 0,
              "resolve the upper texture-slot boundary in both banks");
    }
    check(rejects([&]{ const std::array values{resource(4096)};
                       static_cast<void>(off::data::PictureTextureBindings::build(values, catalog)); }),
          "reject out-of-range key");
    check(rejects([&]{ const std::array values{resource(8)};
                       static_cast<void>(off::data::PictureTextureBindings::build(values, catalog)); }),
          "reject missing image");
    check(rejects([&]{ const std::array values{resource(7), resource(2055)};
                       static_cast<void>(off::data::PictureTextureBindings::build(values, catalog)); }),
          "reject cross-bank alias");
    check(rejects([&]{ static_cast<void>(off::data::PictureTextureBindings::build(direct, catalog, true)); }),
          "enforce upper-bank policy");
    const auto sequence_data = catalog_bytes(7, true);
    const auto sequence_catalog = off::data::TextureCatalog::parse(sequence_data);
    check(rejects([&]{ static_cast<void>(off::data::PictureTextureBindings::build(direct, sequence_catalog)); }),
          "reject unresolved sequence selection");

    const std::array descriptors{
        off::data::PictureResourceDescriptor{
            .local_center_x = 10.0F, .local_center_y = 20.0F, .local_z = 3.0F,
            .u_min = 0.1F, .u_max = 0.4F, .v_max = 0.8F, .v_min = 0.2F,
            .horizontal_edge_span = 8.0F, .vertical_edge_span = 6.0F,
            .modulation_color = 0xff010203U},
        off::data::PictureResourceDescriptor{
            .local_center_x = 30.0F, .local_center_y = 40.0F,
            .horizontal_edge_span = 2.0F, .vertical_edge_span = 4.0F,
            .modulation_color = 0xffaabbccU},
        off::data::PictureResourceDescriptor{
            .local_center_x = 50.0F, .local_center_y = 60.0F,
            .u_min = 0.3F, .u_max = 0.7F, .v_max = 0.9F, .v_min = 0.4F,
            .horizontal_edge_span = 10.0F, .vertical_edge_span = 12.0F,
            .modulation_color = 0xff556677U},
    };
    const std::array groups{
        off::data::PictureDrawGroup{.descriptor_span_count = 2,
                                    .first_descriptor_index = 0},
        off::data::PictureDrawGroup{.descriptor_span_count = 1,
                                    .first_descriptor_index = 2},
    };
    const std::array plan_textures{
        direct_join.entries()[0], upper_join.entries()[0]};
    const auto plan = off::data::PictureDrawPlan::build(
        descriptors, groups, plan_textures);
    check(plan.groups().size() == 2 &&
              plan.groups()[0].texture.bank ==
                  off::data::TextureManagerKeyBank::direct &&
              plan.groups()[1].texture.bank ==
                  off::data::TextureManagerKeyBank::upper &&
              plan.groups()[0].quads.size() == 2 &&
              plan.groups()[0].quads[0].descriptor_index == 0 &&
              plan.groups()[0].quads[1].descriptor_index == 1 &&
              plan.groups()[1].quads[0].descriptor_index == 2 &&
              plan.groups()[0].quads[0].local_x_min == 6.0F &&
              plan.groups()[0].quads[0].local_x_max == 14.0F &&
              plan.groups()[0].quads[0].local_y_min == 17.0F &&
              plan.groups()[0].quads[0].local_y_max == 23.0F &&
              plan.groups()[0].quads[0].local_z == 3.0F &&
              plan.groups()[0].quads[0].u_min == 0.1F &&
              plan.groups()[0].quads[0].u_max == 0.4F &&
              plan.groups()[0].quads[0].v_max == 0.8F &&
              plan.groups()[0].quads[0].v_min == 0.2F &&
              plan.groups()[0].quads[0].modulation_color == 0xff010203U,
          "preserve serialized group and descriptor order in the neutral quad plan");
    check(rejects([&]{
              static_cast<void>(off::data::PictureDrawPlan::build(
                  descriptors, groups, std::span<const off::data::PictureTextureBinding>{}));
          }), "reject mismatched draw-group and texture-binding counts");
    const std::array bad_group{off::data::PictureDrawGroup{
        .descriptor_span_count = static_cast<std::size_t>(-1),
        .first_descriptor_index = 1}};
    const std::array one_texture{direct_join.entries()[0]};
    check(rejects([&]{
              static_cast<void>(off::data::PictureDrawPlan::build(
                  descriptors, bad_group, one_texture));
          }), "reject hostile draw-plan spans without addition overflow");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
