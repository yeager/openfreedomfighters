#include "off/graphics/picture_color_state.hpp"
#include "off/graphics/picture_expansion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
using off::graphics::PictureColorState;
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "invalid picture view must reject");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<PictureColorState> &&
                  !std::is_move_constructible_v<PictureColorState> &&
                  !std::is_copy_assignable_v<PictureColorState> &&
                  !std::is_move_assignable_v<PictureColorState>);
    std::array<off::data::PictureResourceDescriptor, 3> descriptors{};
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        auto& descriptor = descriptors[i];
        descriptor.local_center_x = static_cast<float>(i + 2U);
        descriptor.local_center_y = 3;
        descriptor.local_z = 4;
        descriptor.horizontal_edge_span = 8;
        descriptor.vertical_edge_span = 6;
        descriptor.u_min = 0.125F; descriptor.u_max = 0.875F;
        descriptor.v_min = 0.25F; descriptor.v_max = 0.75F;
        descriptor.modulation_color = 0xa0123456U + static_cast<std::uint32_t>(i);
        descriptor.encoded.fill(static_cast<std::byte>(0x40U + i));
    }
    const auto authored = descriptors;
    std::uint32_t first_material = 0x11111111U;
    std::uint32_t second_material = 0x22222222U;
    std::array<std::uint32_t*, 3> materials{&first_material, &second_material, &first_material};
    PictureColorState owner(9, 0x0913579bU, 0xabcdef00U, descriptors, materials);
    check(owner.alpha() == 9 && owner.color() == 0x0913579bU && owner.material() == 0xabcdef00U &&
          first_material == 0x11111111U && descriptors[0].modulation_color == authored[0].modulation_color,
          "construction preserves explicit initial states without synthetic propagation");
    materials[0] = nullptr; // The ordered pointer collection is owned, its referents stay shared.
    owner.refresh_material(2);
    check(owner.material() == 0x60014U && first_material == 0x60014U && second_material == 0x60014U &&
          owner.color() == 0x0913579bU && descriptors[1].modulation_color == authored[1].modulation_color,
          "material refresh updates duplicate paired identities without changing colors");

    const std::array<off::data::PictureDrawGroup, 1> groups{{{1, 1}}};
    std::array<off::data::PictureTextureBinding, 1> bindings{};
    bindings[0].prm_offset = 789; bindings[0].manager_key = 0x812;
    bindings[0].texture_id = 18; bindings[0].image_index = 5;
    bindings[0].authored_texture_resource_record.fill(std::byte{0x6a});
    const auto old_plan = owner.draw_plan(groups, bindings);
    owner.set_alpha(254);
    check(owner.alpha() == 254 && owner.color() == 0xfe13579bU && owner.material() == 0x60015U &&
          first_material == 0x60015U && second_material == 0x60015U,
          "alpha setter changes only material bit zero and preserves owner RGB");
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        check(descriptors[i].modulation_color == 0xfe13579bU && descriptors[i].encoded == authored[i].encoded &&
              descriptors[i].local_center_x == authored[i].local_center_x &&
              descriptors[i].horizontal_edge_span == authored[i].horizontal_edge_span &&
              descriptors[i].u_min == authored[i].u_min && descriptors[i].v_max == authored[i].v_max,
              "all descriptors including ungrouped receive whole owner color, not geometry or encoded-byte changes");
    }
    const auto new_plan = owner.draw_plan(groups, bindings);
    const auto& new_group = new_plan.groups().front();
    check(old_plan.groups().front().quads.front().modulation_color == authored[1].modulation_color &&
          new_group.quads.front().modulation_color == 0xfe13579bU &&
          new_group.texture.prm_offset == 789 && new_group.texture.manager_key == 0x812 &&
          new_group.texture.texture_id == 18 && new_group.texture.image_index == 5 &&
          new_group.texture.authored_texture_resource_record == bindings[0].authored_texture_resource_record,
          "fresh draw plan consumes runtime colors and retains complete texture identity");
    const off::graphics::PictureCacheTransform transform{
        .basis = {1, 0, 0, 0, 1, 0, 0, 0, 1}, .translation = {0, 0, 0}};
    const auto expanded = off::graphics::expand_picture_descriptors(new_group.quads, transform);
    check(expanded.size() == 1 && expanded.front().vertices.size() == 4, "current plan expands actual quad");
    for (const auto& vertex : expanded.front().vertices)
        check(vertex.color == 0x7f092b4dU, "color reduction occurs exactly once downstream of mutation");

    first_material = 0xdeadbeefU; second_material = 0x12345678U;
    descriptors[0].modulation_color = 0;
    owner.set_alpha(254);
    check(first_material == 0xdeadbeefU && second_material == 0x12345678U &&
          descriptors[0].modulation_color == 0xfe13579bU,
          "same material bit skips paired writes, but repeated alpha still rewrites descriptor colors");
    owner.set_alpha(255);
    check(owner.material() == 0x60014U && first_material == 0x60014U && owner.color() == 0xff13579bU,
          "full argument 255 clears material bit");
    owner.set_alpha(511);
    check(owner.alpha() == 255 && owner.color() == 0xff13579bU && owner.material() == 0x60015U &&
          second_material == 0x60015U, "full argument comparison precedes low-byte truncation");
    owner.set_alpha(256);
    check(owner.alpha() == 0 && owner.color() == 0x0013579bU, "high argument bits do not leak into packed color");

    const std::array<std::uint32_t*, 1> shared{&first_material};
    PictureColorState other(255, 0xff2468acU, 0x76543210U, descriptors, shared);
    other.set_alpha(7);
    check(descriptors[1].modulation_color == 0x072468acU && first_material == 0x76543211U &&
          owner.color() == 0x0013579bU && owner.material() == 0x60015U,
          "shared storage aliases observe mutations while distinct owner state stays distinct");
    check(owner.draw_plan(groups, bindings).groups().front().quads.front().modulation_color == 0x072468acU,
          "another owner draw snapshot reads shared descriptor changes");
    owner.refresh_material(0x87654320U);
    check(first_material == 0x87654320U && second_material == 0x87654320U &&
          descriptors[0].modulation_color == 0x072468acU,
          "explicit refresh propagates passthrough material without alpha override or descriptor writes");
    PictureColorState absent(255, 0xff010203U, 0, {}, {});
    absent.set_alpha(3);
    check(absent.alpha() == 3 && absent.color() == 0x03010203U && absent.material() == 1,
          "absent descriptors still update owner alpha color and material");
    const std::array<std::uint32_t*, 2> invalid{&first_material, nullptr};
    const auto retained_material = first_material;
    rejects([&] { PictureColorState bad(0, 0, 0, descriptors, invalid); });
    check(first_material == retained_material, "invalid borrowed material pointer fails without writes");
    rejects([&] { static_cast<void>(owner.draw_plan(groups, {})); });
    return failures == 0 ? 0 : 1;
}
