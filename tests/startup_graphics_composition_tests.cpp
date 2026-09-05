#include "off/data/startup_graphics_composition.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

struct Fixture {
    std::vector<off::data::GmsDirectoryEntry> directory;
    std::vector<off::data::GmsHierarchyNode> hierarchy;

    std::size_t add(std::optional<std::size_t> parent, std::uint32_t type,
                    float x, float y) {
        const auto index = directory.size();
        off::data::GmsDirectoryEntry entry;
        entry.source_type = type;
        entry.position = {x, y, 0};
        directory.push_back(entry);
        hierarchy.push_back({index, parent, {}});
        if (parent) hierarchy[*parent].children_in_directory_order.push_back(index);
        return index;
    }
};

Fixture valid_fixture() {
    Fixture f;
    const auto menu = f.add({}, 0x0010002eU, 0, 0);
    for (std::size_t action_index = 0; action_index < 2; ++action_index) {
        const auto action = f.add(menu, 0x00100033U, 60, 400);
        f.add(action, 0x0020002dU, 0, 0);
        f.add(action, 0x0020002dU, 0, 0);
        f.add(action, 0x0020002dU, 0, 0);
    }
    const auto settings = f.add(menu, 0x0010002eU, 260, 185);
    f.add(menu, 0x0020002dU, 60, 155);
    constexpr float ys[]{0, 18, 36, 54, 72, 90, 108, 108};
    for (std::size_t row_index = 0; row_index < std::size(ys); ++row_index) {
        const auto y = ys[row_index];
        const auto row = f.add(settings, 0x00100034U, 0, y);
        const auto control = f.add(row, 0x00100033U, 0, -2);
        f.add(control, 0x00200046U, 0, 0);
        f.add(control, 0x00200046U, 0, 0);
        f.add(row, 0x0020002dU, -200, 0);
        f.add(row, 0x0020002dU, -200, 0);
        if (row_index == 3) f.add(row, 0x0020002dU, -200, 0);
        f.add(row, 0x0020002dU, 140, 0);
        f.add(row, 0x0020002dU, 140, 0);
        f.add(row, 0x00200046U, 0, -7);
    }
    return f;
}

off::data::PictureDrawPlan plan(std::size_t count, std::size_t image_bias = 0,
                                std::size_t descriptor_bias = 0,
                                bool collapse_images = false) {
    std::vector<off::data::PictureResourceDescriptor> descriptors(count + descriptor_bias);
    std::vector<off::data::PictureDrawGroup> groups;
    std::vector<off::data::PictureTextureBinding> bindings;
    for (std::size_t i = 0; i < count; ++i) {
        auto& descriptor = descriptors[i + descriptor_bias];
        descriptor.local_center_x = static_cast<float>(i);
        descriptor.horizontal_edge_span = 2.0F;
        descriptor.vertical_edge_span = 2.0F;
        descriptor.modulation_color = 0xffffffffU;
        groups.push_back({1, i + descriptor_bias});
        bindings.push_back({0, static_cast<std::uint16_t>(2048 + i),
                            static_cast<std::uint16_t>(i),
                            image_bias + (collapse_images ? 0 : i),
                            off::data::TextureManagerKeyBank::upper});
    }
    return off::data::PictureDrawPlan::build(descriptors, groups, bindings);
}

std::array<off::data::StartupGraphicsRowComposition, 8> valid_rows() {
    std::array<off::data::StartupGraphicsRowComposition, 8> rows;
    const auto background = plan(1, 50);
    const auto chrome = plan(5, 60);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto& row = rows[i];
        row.owner_directory_index = 100 + i;
        row.slot_y = static_cast<float>(i < 6 ? i * 18 : 108);
        row.same_slot_multiplicity = i < 6 ? 1 : 2;
        row.same_slot_ordinal = i < 6 ? 0 : i - 6;
        row.construction_chain = {row.owner_directory_index};
        row.transform_chain = {{row.owner_directory_index, {}, {}}};
        for (std::size_t picture = 0; picture < 3; ++picture) {
            auto& instance = row.pictures[picture];
            instance.role = picture == 0
                ? off::data::StartupGraphicsCompositionRole::row_background
                : off::data::StartupGraphicsCompositionRole::row_chrome;
            instance.directory_index = 200 + i * 3 + picture;
            instance.construction_chain = {row.owner_directory_index,
                                           instance.directory_index};
            instance.transform_chain = {
                {row.owner_directory_index, {}, {}},
                {instance.directory_index, {}, {}}};
            instance.draw_plan = picture == 0 ? background : chrome;
        }
    }
    return rows;
}
}  // namespace

int main() {
    const auto rejects = [](const Fixture& value) {
        try {
            static_cast<void>(off::data::StartupGraphicsComposition::locate(
                value.directory, value.hierarchy));
            return false;
        } catch (const std::runtime_error&) { return true; }
    };
    auto fixture = valid_fixture();
    const auto rows = off::data::StartupGraphicsComposition::locate(
        fixture.directory, fixture.hierarchy);
    check(rows.size() == 8, "locate eight neutral row owners");
    for (const auto& row : rows) {
        check(row.background_directory_index != row.chrome_directory_indexes[0] &&
              row.chrome_directory_indexes[0] != row.chrome_directory_indexes[1],
              "keep three distinct composition instances");
    }
    auto negative_zero = fixture;
    negative_zero.directory[0].position[0] = -0.0F;
    check(off::data::StartupGraphicsComposition::locate(
              negative_zero.directory, negative_zero.hierarchy).size() == 8,
          "treat negative and positive zero as the same authored coordinate");
    auto nextafter_coordinate = fixture;
    nextafter_coordinate.directory[0].position[0] =
        std::nextafter(0.0F, 1.0F);
    check(rejects(nextafter_coordinate), "reject a nextafter coordinate mismatch");
    auto non_finite = fixture;
    non_finite.directory.back().position[0] =
        std::numeric_limits<float>::infinity();
    check(rejects(non_finite), "reject non-finite 2D coordinates globally");

    auto bad_identity = fixture;
    bad_identity.hierarchy[1].directory_index = 2;
    check(rejects(bad_identity), "reject hierarchy node identity mismatch");
    auto bad_parent = fixture;
    bad_parent.hierarchy[1].parent_directory_index = 1;
    check(rejects(bad_parent), "reject a non-preceding parent");
    auto duplicate_child = fixture;
    duplicate_child.hierarchy[0].children_in_directory_order.push_back(
        duplicate_child.hierarchy[0].children_in_directory_order.back());
    check(rejects(duplicate_child), "reject duplicate or unordered children");
    auto missing_child = fixture;
    missing_child.hierarchy[0].children_in_directory_order.erase(
        missing_child.hierarchy[0].children_in_directory_order.begin());
    check(rejects(missing_child), "reject a non-root omitted from its parent");
    auto root_referenced = fixture;
    root_referenced.hierarchy[1].children_in_directory_order.push_back(0);
    check(rejects(root_referenced), "reject a root referenced as a child");
    auto malformed_action = fixture;
    const auto action = malformed_action.hierarchy[0].children_in_directory_order[0];
    const auto action_child = malformed_action.hierarchy[action].children_in_directory_order[0];
    malformed_action.directory[action_child].source_type = 0x00200046U;
    check(rejects(malformed_action), "reject malformed action-control child shape");
    auto malformed = fixture;
    malformed.directory[rows[0].background_directory_index].position[1] = -6;
    check(rejects(malformed), "reject a composition shape mismatch");

    auto ambiguous = fixture;
    const auto offset = ambiguous.directory.size();
    for (const auto& entry : fixture.directory) ambiguous.directory.push_back(entry);
    for (const auto& node : fixture.hierarchy) {
        auto copy = node;
        copy.directory_index += offset;
        if (copy.parent_directory_index) *copy.parent_directory_index += offset;
        for (auto& child : copy.children_in_directory_order) child += offset;
        ambiguous.hierarchy.push_back(std::move(copy));
    }
    check(rejects(ambiguous), "reject multiple structural matches");

    const auto factory_rejects = [](auto rows) {
        try {
            static_cast<void>(
                off::data::StartupGraphicsComposition::from_rows(std::move(rows)));
            return false;
        } catch (const std::runtime_error&) { return true; }
    };
    auto rows_value = valid_rows();
    const auto owned = off::data::StartupGraphicsComposition::from_rows(rows_value);
    rows_value[0].slot_y = 999.0F;
    check(owned.rows()[0].slot_y == 0.0F,
          "factory owns row values independently of its input");
    auto bad_role = valid_rows();
    bad_role[0].pictures[0].role =
        off::data::StartupGraphicsCompositionRole::row_chrome;
    check(factory_rejects(bad_role), "factory rejects role mismatch");
    auto bad_group_count = valid_rows();
    bad_group_count[0].pictures[1].draw_plan = plan(4, 60);
    check(factory_rejects(bad_group_count), "factory rejects group-count mismatch");
    auto bad_unit_order = valid_rows();
    bad_unit_order[0].pictures[1].draw_plan = plan(5, 60, 1);
    check(factory_rejects(bad_unit_order), "factory rejects noncanonical descriptor order");
    auto bad_signature = valid_rows();
    bad_signature[7].pictures[2].draw_plan = plan(5, 70);
    check(factory_rejects(bad_signature), "factory rejects canonical image signature mismatch");
    auto collapsed_images = valid_rows();
    const auto collapsed_chrome = plan(5, 60, 0, true);
    for (auto& row : collapsed_images)
        row.pictures[1].draw_plan = row.pictures[2].draw_plan = collapsed_chrome;
    check(factory_rejects(collapsed_images), "factory rejects collapsed chrome image identities");
    auto colliding_background = valid_rows();
    const auto colliding_chrome = plan(5, 50);
    for (auto& row : colliding_background)
        row.pictures[1].draw_plan = row.pictures[2].draw_plan = colliding_chrome;
    check(factory_rejects(colliding_background), "factory rejects background/chrome image collision");
    auto bad_row_chain = valid_rows();
    bad_row_chain[2].transform_chain[0].directory_index += 1;
    check(factory_rejects(bad_row_chain), "factory rejects row chain identity mismatch");
    auto bad_picture_chain = valid_rows();
    bad_picture_chain[2].pictures[1].transform_chain.pop_back();
    check(factory_rejects(bad_picture_chain), "factory rejects picture chain length mismatch");
    auto bad_slot = valid_rows();
    bad_slot[7].same_slot_ordinal = 0;
    check(factory_rejects(bad_slot), "factory rejects slot ordinal mismatch");
    auto bad_slot_y = valid_rows();
    bad_slot_y[4].slot_y = std::nextafter(72.0F, 73.0F);
    check(factory_rejects(bad_slot_y), "factory rejects slot coordinate mismatch");
    auto bad_endpoint = valid_rows();
    bad_endpoint[1].pictures[0].construction_chain.back() += 1;
    bad_endpoint[1].pictures[0].transform_chain.back().directory_index += 1;
    check(factory_rejects(bad_endpoint), "factory rejects construction endpoint mismatch");
    std::cout << "startup graphics composition tests passed\n";
}
