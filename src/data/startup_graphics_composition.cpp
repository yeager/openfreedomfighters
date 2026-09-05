#include "off/data/startup_graphics_composition.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <unordered_set>

namespace off::data {
namespace {

constexpr std::uint32_t overlay_type = 0x0010002eU;
constexpr std::uint32_t control_type = 0x00100033U;
constexpr std::uint32_t row_type = 0x00100034U;
constexpr std::uint32_t text_type = 0x0020002dU;
constexpr std::uint32_t picture_type = 0x00200046U;
constexpr std::uint32_t authored_hide_flag = 0x00000400U;
constexpr std::uint8_t resting_state = 0x01U;
constexpr std::uint8_t persistent_state_mask = 0x80U;
// This is the narrow recovered allowed-state mask for the startup graphics
// control family, not a general window-state mask.
constexpr std::uint8_t recovered_allowed_states =
    resting_state | 0x08U | 0x10U | 0x20U | persistent_state_mask;

bool at(const GmsDirectoryEntry& entry, float x, float y) {
    return entry.position[0] == x && entry.position[1] == y;
}

std::vector<std::size_t> chain_for(
    std::span<const GmsDirectoryEntry> directory,
    std::span<const GmsHierarchyNode> hierarchy,
    std::size_t index
) {
    std::vector<std::size_t> reversed;
    while (true) {
        if (index >= directory.size() || index >= hierarchy.size()) {
            throw std::runtime_error("startup graphics hierarchy index is out of range");
        }
        reversed.push_back(index);
        if (!hierarchy[index].parent_directory_index.has_value()) break;
        const auto parent = *hierarchy[index].parent_directory_index;
        if (parent >= index) {
            throw std::runtime_error("startup graphics hierarchy parent is not preceding");
        }
        index = parent;
    }
    std::ranges::reverse(reversed);
    return reversed;
}

bool children_are_valid(std::span<const GmsHierarchyNode> hierarchy,
                        std::size_t parent) {
    return parent < hierarchy.size() &&
        std::ranges::all_of(hierarchy[parent].children_in_directory_order,
            [&](std::size_t child) {
                return child < hierarchy.size() &&
                    hierarchy[child].parent_directory_index == parent;
            });
}

void validate_hierarchy(std::span<const GmsDirectoryEntry> directory,
                        std::span<const GmsHierarchyNode> hierarchy) {
    if (directory.size() != hierarchy.size())
        throw std::runtime_error("startup graphics hierarchy size does not match directory");
    std::vector<std::size_t> child_references(directory.size());
    for (std::size_t index = 0; index < hierarchy.size(); ++index) {
        const auto& node = hierarchy[index];
        if (node.directory_index != index)
            throw std::runtime_error("startup graphics hierarchy node identity mismatch");
        if (node.parent_directory_index && *node.parent_directory_index >= index)
            throw std::runtime_error("startup graphics hierarchy parent is not preceding");
        std::size_t previous = 0;
        bool first = true;
        for (const auto child : node.children_in_directory_order) {
            if (child >= hierarchy.size() || child <= index ||
                (!first && child <= previous) ||
                hierarchy[child].parent_directory_index != index)
                throw std::runtime_error("startup graphics hierarchy child list is invalid");
            ++child_references[child];
            previous = child;
            first = false;
        }
    }
    for (std::size_t index = 0; index < hierarchy.size(); ++index) {
        const auto expected = hierarchy[index].parent_directory_index ? 1U : 0U;
        if (child_references[index] != expected)
            throw std::runtime_error("startup graphics hierarchy parent links are inconsistent");
    }
}

bool same_quad(const PictureQuad& a, const PictureQuad& b) {
    return a.local_x_min == b.local_x_min && a.local_x_max == b.local_x_max &&
        a.local_y_min == b.local_y_min && a.local_y_max == b.local_y_max &&
        a.local_z == b.local_z && a.u_min == b.u_min && a.u_max == b.u_max &&
        a.v_min == b.v_min && a.v_max == b.v_max &&
        a.modulation_color == b.modulation_color &&
        a.descriptor_index == b.descriptor_index;
}

bool same_plan(const PictureDrawPlan& a, const PictureDrawPlan& b) {
    if (a.groups().size() != b.groups().size()) return false;
    for (std::size_t i = 0; i < a.groups().size(); ++i) {
        const auto& x = a.groups()[i];
        const auto& y = b.groups()[i];
        if (x.texture.image_index != y.texture.image_index ||
            x.texture.texture_id != y.texture.texture_id ||
            x.texture.manager_key != y.texture.manager_key ||
            x.texture.bank != y.texture.bank || x.quads.size() != y.quads.size())
            return false;
        for (std::size_t q = 0; q < x.quads.size(); ++q)
            if (!same_quad(x.quads[q], y.quads[q])) return false;
    }
    return true;
}

}  // namespace

std::array<StartupGraphicsRowLocation, 8> StartupGraphicsComposition::locate(
    std::span<const GmsDirectoryEntry> directory,
    std::span<const GmsHierarchyNode> hierarchy
) {
    validate_hierarchy(directory, hierarchy);
    for (const auto& source : directory) {
        if (!std::isfinite(source.position[0]) || !std::isfinite(source.position[1]))
            throw std::runtime_error("startup graphics 2D coordinate is not finite");
    }
    std::vector<std::array<StartupGraphicsRowLocation, 8>> matches;
    for (std::size_t menu = 0; menu < directory.size(); ++menu) {
        if (directory[menu].source_type != overlay_type || !at(directory[menu], 0, 0) ||
            !children_are_valid(hierarchy, menu)) continue;
        const auto& menu_children = hierarchy[menu].children_in_directory_order;
        if (menu_children.size() != 4) continue;
        std::vector<std::size_t> action_controls;
        std::vector<std::size_t> settings_candidates;
        std::size_t header_count = 0;
        for (const auto child : menu_children) {
            const auto& source = directory[child];
            if (source.source_type == control_type && at(source, 60, 400)) {
                action_controls.push_back(child);
            } else if (source.source_type == overlay_type && at(source, 260, 185)) {
                settings_candidates.push_back(child);
            } else if (source.source_type == text_type && at(source, 60, 155)) {
                ++header_count;
            }
        }
        if (action_controls.size() != 2 || settings_candidates.size() != 1 ||
            header_count != 1) continue;
        if (!std::ranges::all_of(action_controls, [&](std::size_t action) {
                if (!children_are_valid(hierarchy, action) ||
                    hierarchy[action].children_in_directory_order.size() != 3) return false;
                return std::ranges::all_of(
                    hierarchy[action].children_in_directory_order,
                    [&](std::size_t child) {
                        return directory[child].source_type == text_type &&
                            at(directory[child], 0, 0);
                    });
            })) continue;
        const auto settings = settings_candidates.front();
        if (!children_are_valid(hierarchy, settings)) continue;
        if (hierarchy[settings].children_in_directory_order.size() != 8) continue;
        std::vector<std::size_t> owners;
        for (const auto child : hierarchy[settings].children_in_directory_order) {
            if (directory[child].source_type == row_type && directory[child].position[0] == 0) {
                owners.push_back(child);
            }
        }
        if (owners.size() != 8) continue;
        auto ordered = owners;
        std::ranges::sort(ordered, [&](std::size_t a, std::size_t b) {
            if (directory[a].position[1] != directory[b].position[1])
                return directory[a].position[1] < directory[b].position[1];
            return a < b;
        });
        std::array<float, 8> ys{};
        std::ranges::transform(ordered, ys.begin(), [&](std::size_t i) {
            return directory[i].position[1];
        });
        if (ys != std::array<float, 8>{0, 18, 36, 54, 72, 90, 108, 108}) continue;

        std::array<StartupGraphicsRowLocation, 8> rows{};
        bool valid = true;
        std::array<std::size_t, 8> left_text_counts{};
        for (std::size_t row_index = 0; row_index < ordered.size(); ++row_index) {
            const auto owner = ordered[row_index];
            if (!children_are_valid(hierarchy, owner)) { valid = false; break; }
            std::vector<std::size_t> controls;
            std::vector<std::size_t> backgrounds;
            std::size_t left_text = 0;
            std::size_t right_text = 0;
            for (const auto child : hierarchy[owner].children_in_directory_order) {
                const auto& source = directory[child];
                if (source.source_type == control_type && at(source, 0, -2))
                    controls.push_back(child);
                else if (source.source_type == picture_type && at(source, 0, -7))
                    backgrounds.push_back(child);
                else if (source.source_type == text_type && at(source, -200, 0))
                    ++left_text;
                else if (source.source_type == text_type && at(source, 140, 0))
                    ++right_text;
            }
            if (controls.size() != 1 || backgrounds.size() != 1 ||
                (left_text != 2 && left_text != 3) || right_text != 2 ||
                hierarchy[owner].children_in_directory_order.size() !=
                    4U + left_text ||
                !children_are_valid(hierarchy, controls.front())) {
                valid = false; break;
            }
            left_text_counts[row_index] = left_text;
            std::vector<std::size_t> chromes;
            for (const auto child : hierarchy[controls.front()].children_in_directory_order) {
                if (directory[child].source_type == picture_type && at(directory[child], 0, 0))
                    chromes.push_back(child);
            }
            if (chromes.size() != 2 ||
                hierarchy[controls.front()].children_in_directory_order.size() != 2) {
                valid = false; break;
            }
            rows[row_index] = {owner, backgrounds.front(), {chromes[0], chromes[1]}};
        }
        std::ranges::sort(left_text_counts);
        if (left_text_counts !=
            std::array<std::size_t, 8>{2, 2, 2, 2, 2, 2, 2, 3}) valid = false;
        if (valid) matches.push_back(rows);
    }
    if (matches.size() != 1) {
        throw std::runtime_error("startup graphics subtree does not have exactly one structural match");
    }
    return matches.front();
}

StartupGraphicsComposition StartupGraphicsComposition::build(
    const GmsImage& image,
    std::span<const std::byte> primitive_allocation,
    const TextureCatalog& textures
) {
    const auto locations = locate(image.directory(), image.hierarchy());
    std::array<StartupGraphicsRowComposition, 8> built_rows;
    for (std::size_t row_index = 0; row_index < locations.size(); ++row_index) {
        const auto& location = locations[row_index];
        auto make_chain = [&](std::size_t index) {
            const auto indexes = chain_for(image.directory(), image.hierarchy(), index);
            std::vector<StartupGraphicsLocalTransform> transforms;
            transforms.reserve(indexes.size());
            for (const auto part : indexes) transforms.push_back(
                {part, image.directory()[part].basis, image.directory()[part].position});
            return std::pair{indexes, transforms};
        };
        auto [row_chain, row_transforms] = make_chain(location.owner_directory_index);
        StartupGraphicsRowComposition row;
        row.owner_directory_index = location.owner_directory_index;
        row.slot_y = image.directory()[location.owner_directory_index].position[1];
        row.same_slot_multiplicity = static_cast<std::size_t>(std::ranges::count_if(
            locations, [&](const auto& other) {
                return image.directory()[other.owner_directory_index].position[1] == row.slot_y;
            }));
        row.same_slot_ordinal = static_cast<std::size_t>(std::ranges::count_if(
            locations | std::views::take(row_index), [&](const auto& other) {
                return image.directory()[other.owner_directory_index].position[1] == row.slot_y;
            }));
        row.authored_hidden =
            (image.directory()[location.owner_directory_index].object_flags &
             authored_hide_flag) != 0;
        row.construction_chain = std::move(row_chain);
        row.transform_chain = std::move(row_transforms);
        const std::array<std::size_t, 3> indexes{
            location.background_directory_index,
            location.chrome_directory_indexes[0],
            location.chrome_directory_indexes[1]};
        for (std::size_t picture_index = 0; picture_index < indexes.size(); ++picture_index) {
            const auto directory_index = indexes[picture_index];
            const auto source = image.startup_window_picture_source(directory_index);
            if (source.authored_state_exponent >=
                std::numeric_limits<std::uint8_t>::digits) {
                throw std::runtime_error(
                    "startup graphics picture state exponent is out of range");
            }
            const auto authored_state_mask = static_cast<std::uint8_t>(
                std::uint32_t{1} << source.authored_state_exponent);
            const auto picture = PictureResource::parse(
                primitive_allocation, source.picture_asset_reference);
            const auto expected_groups = picture_index == 0 ? 1U : 5U;
            if (picture.draw_groups().size() != expected_groups ||
                picture.descriptors().size() != expected_groups ||
                picture.texture_resources().size() != expected_groups) {
                throw std::runtime_error("startup graphics picture composition shape mismatch");
            }
            for (std::size_t group = 0; group < picture.draw_groups().size(); ++group) {
                if (picture.draw_groups()[group].descriptor_span_count != 1 ||
                    picture.draw_groups()[group].first_descriptor_index != group)
                    throw std::runtime_error("startup graphics picture groups are not unit ordered spans");
            }
            const auto bindings = PictureTextureBindings::build(
                picture.texture_resources(), textures, true);
            auto [chain, transforms] = make_chain(directory_index);
            row.pictures[picture_index] = {
                .role = picture_index == 0
                    ? StartupGraphicsCompositionRole::row_background
                    : StartupGraphicsCompositionRole::row_chrome,
                .directory_index = directory_index,
                .construction_chain = std::move(chain),
                .transform_chain = std::move(transforms),
                .picture_reference = source.picture_asset_reference,
                .authored_state_mask = authored_state_mask,
                .base_render_property = source.base_render_property,
                .authored_alpha = source.authored_alpha,
                .alignment_enum = source.alignment_enum,
                .extension_control = source.extension_control,
                .draw_plan = PictureDrawPlan::build(picture, bindings)};
        }
        built_rows[row_index] = std::move(row);
    }
    return from_rows(std::move(built_rows));
}

StartupGraphicsComposition StartupGraphicsComposition::from_rows(
    std::array<StartupGraphicsRowComposition, 8> rows
) {
    StartupGraphicsComposition result;
    result.rows_ = std::move(rows);
    const auto& background = result.rows_[0].pictures[0].draw_plan;
    const auto& chrome = result.rows_[0].pictures[1].draw_plan;
    if (background.groups().size() != 1 || chrome.groups().size() != 5)
        throw std::runtime_error("startup graphics canonical group counts mismatch");
    std::unordered_set<std::size_t> image_identities;
    image_identities.insert(background.groups()[0].texture.image_index);
    for (const auto& group : chrome.groups()) image_identities.insert(group.texture.image_index);
    if (image_identities.size() != 6)
        throw std::runtime_error("startup graphics composition does not use six distinct catalog images");
    std::unordered_set<std::size_t> row_identities;
    std::unordered_set<std::size_t> picture_identities;
    for (std::size_t row_index = 0; row_index < result.rows_.size(); ++row_index) {
        const auto& row = result.rows_[row_index];
        if (!row_identities.insert(row.owner_directory_index).second)
            throw std::runtime_error("startup graphics row identity is duplicated");
        if (row.pictures[0].role != StartupGraphicsCompositionRole::row_background ||
            row.pictures[1].role != StartupGraphicsCompositionRole::row_chrome ||
            row.pictures[2].role != StartupGraphicsCompositionRole::row_chrome ||
            row.pictures[0].draw_plan.groups().size() != 1 ||
            row.pictures[1].draw_plan.groups().size() != 5 ||
            row.pictures[2].draw_plan.groups().size() != 5)
            throw std::runtime_error("startup graphics row composition roles or group counts mismatch");
        if (row.pictures[0].authored_state_mask != persistent_state_mask ||
            row.pictures[1].authored_state_mask != resting_state ||
            row.pictures[2].authored_state_mask != resting_state)
            throw std::runtime_error("startup graphics picture state-mask contract mismatch");
        for (const auto& picture : row.pictures) {
            if (picture.alignment_enum > 15U ||
                (picture.extension_control.has_value() &&
                 *picture.extension_control > 16U))
                throw std::runtime_error(
                    "startup graphics picture authored controls are out of range");
            if (!picture_identities.insert(picture.directory_index).second)
                throw std::runtime_error("startup graphics picture identity is duplicated");
            for (std::size_t group_index = 0;
                 group_index < picture.draw_plan.groups().size(); ++group_index) {
                const auto& group = picture.draw_plan.groups()[group_index];
                if (group.quads.size() != 1 ||
                    group.quads[0].descriptor_index != group_index)
                    throw std::runtime_error("startup graphics draw plan is not unit ordered");
            }
        }
        if (!same_plan(row.pictures[0].draw_plan, background) ||
            !same_plan(row.pictures[1].draw_plan, chrome) ||
            !same_plan(row.pictures[2].draw_plan, chrome))
            throw std::runtime_error("startup graphics picture signature mismatch");
        if (row.construction_chain.size() != row.transform_chain.size())
            throw std::runtime_error("startup graphics row transform chain mismatch");
        for (std::size_t i = 0; i < row.construction_chain.size(); ++i)
            if (row.construction_chain[i] != row.transform_chain[i].directory_index)
                throw std::runtime_error("startup graphics row transform identity mismatch");
        for (const auto& picture : row.pictures) {
            if (picture.construction_chain.size() != picture.transform_chain.size())
                throw std::runtime_error("startup graphics picture transform chain mismatch");
            for (std::size_t i = 0; i < picture.construction_chain.size(); ++i)
                if (picture.construction_chain[i] != picture.transform_chain[i].directory_index)
                    throw std::runtime_error("startup graphics picture transform identity mismatch");
        }
    }
    for (std::size_t i = 0; i < result.rows_.size(); ++i) {
        constexpr std::array<float, 8> expected_y{0, 18, 36, 54, 72, 90, 108, 108};
        const auto expected_multiplicity = i < 6 ? 1U : 2U;
        const auto expected_ordinal = i < 6 ? 0U : i - 6U;
        if (result.rows_[i].slot_y != expected_y[i] ||
            result.rows_[i].same_slot_multiplicity != expected_multiplicity ||
            result.rows_[i].same_slot_ordinal != expected_ordinal)
            throw std::runtime_error("startup graphics row slot identity mismatch");
        if (result.rows_[i].construction_chain.empty() ||
            result.rows_[i].construction_chain.back() !=
                result.rows_[i].owner_directory_index)
            throw std::runtime_error("startup graphics row construction endpoint mismatch");
        for (const auto& picture : result.rows_[i].pictures)
            if (picture.construction_chain.empty() ||
                picture.construction_chain.back() != picture.directory_index)
                throw std::runtime_error("startup graphics picture construction endpoint mismatch");
    }
    const auto hidden_rows = std::ranges::count_if(
        result.rows_, [](const auto& row) { return row.authored_hidden; });
    if (hidden_rows != 1 ||
        (!result.rows_[6].authored_hidden && !result.rows_[7].authored_hidden))
        throw std::runtime_error("startup graphics authored row visibility mismatch");
    return result;
}

StartupGraphicsVisibility StartupGraphicsComposition::visible_pictures(
    std::uint8_t requested_state
) const {
    StartupGraphicsVisibility result;
    result.requested_state = requested_state;
    result.effective_state =
        (requested_state & recovered_allowed_states) != 0
            ? requested_state
            : resting_state;
    result.pictures.reserve(21);
    for (std::size_t row_index = 0; row_index < rows_.size(); ++row_index) {
        const auto& row = rows_[row_index];
        if (row.authored_hidden) continue;
        for (std::size_t picture_index = 0;
             picture_index < row.pictures.size(); ++picture_index) {
            const auto& picture = row.pictures[picture_index];
            const auto persistent =
                (picture.authored_state_mask & persistent_state_mask) != 0;
            if (!persistent &&
                (picture.authored_state_mask & result.effective_state) == 0)
                continue;
            result.pictures.push_back({
                row_index,
                picture_index,
                row.owner_directory_index,
                picture.directory_index,
                picture.role,
                picture.draw_plan.groups().size(),
            });
        }
    }
    return result;
}

StartupGraphicsTraversalPlan StartupGraphicsComposition::traversal_emission_plan(
    std::uint8_t requested_state
) const {
    const auto visibility = visible_pictures(requested_state);
    StartupGraphicsTraversalPlan result;
    result.requested_state = visibility.requested_state;
    result.effective_state = visibility.effective_state;
    result.pictures.reserve(visibility.pictures.size());
    result.group_emissions.reserve(visibility.pictures.size() * 5);

    std::vector<std::size_t> live_rows;
    live_rows.reserve(rows_.size());
    for (std::size_t row_index = 0; row_index < rows_.size(); ++row_index)
        if (!rows_[row_index].authored_hidden) live_rows.push_back(row_index);
    std::ranges::sort(live_rows, std::greater{}, [&](std::size_t row_index) {
        return rows_[row_index].owner_directory_index;
    });

    constexpr std::array<std::size_t, 3> live_picture_order{1, 2, 0};
    for (const auto row_index : live_rows) {
        const auto& row = rows_[row_index];
        for (const auto picture_index : live_picture_order) {
            const auto& picture = row.pictures[picture_index];
            const auto persistent =
                (picture.authored_state_mask & persistent_state_mask) != 0;
            if (!persistent &&
                (picture.authored_state_mask & result.effective_state) == 0)
                continue;
            const auto first_group = result.group_emissions.size();
            result.pictures.push_back({{
                row_index,
                picture_index,
                row.owner_directory_index,
                picture.directory_index,
                picture.role,
                picture.draw_plan.groups().size(),
            }, first_group});
            for (std::size_t group_index = 0;
                 group_index < picture.draw_plan.groups().size(); ++group_index)
                result.group_emissions.push_back({
                    row_index, picture_index, picture.directory_index, group_index});
        }
    }
    return result;
}

}  // namespace off::data
