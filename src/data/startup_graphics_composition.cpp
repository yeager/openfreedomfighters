#include "off/data/startup_graphics_composition.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace off::data {
namespace {

constexpr std::uint32_t overlay_type = 0x0010002eU;
constexpr std::uint32_t window_type = 0x00100031U;
constexpr std::uint32_t control_type = 0x00100033U;
constexpr std::uint32_t row_type = 0x00100034U;
constexpr std::uint32_t text_type = 0x0020002dU;
constexpr std::uint32_t picture_type = 0x00200046U;
constexpr std::uint32_t authored_hide_flag = 0x00000400U;
constexpr std::uint8_t resting_state = 0x01U;
constexpr std::uint8_t persistent_state_mask = 0x80U;
// Supported authored state bits for this UI feature, not a general window mask.
constexpr std::uint8_t allowed_startup_states =
    resting_state | 0x08U | 0x10U | 0x20U | persistent_state_mask;
constexpr std::string_view graphics_menu_component_identifier =
    "ZWINDOW_GraphicsMenu";

bool equal_ascii_case_insensitive(std::string_view left,
                                  std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        auto a = static_cast<unsigned char>(left[index]);
        auto b = static_cast<unsigned char>(right[index]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
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

}  // namespace

std::vector<StartupGraphicsRowLocation> StartupGraphicsComposition::locate(
    std::span<const GmsDirectoryEntry> directory,
    std::span<const GmsHierarchyNode> hierarchy,
    std::span<const std::size_t> registered_window_owner_sources
) {
    validate_hierarchy(directory, hierarchy);
    if (registered_window_owner_sources.size() != 1U ||
        registered_window_owner_sources.front() >= directory.size() ||
        directory[registered_window_owner_sources.front()].source_type != window_type)
        throw std::runtime_error(
            "startup graphics route requires one typed registered window owner");
    const auto registered_owner = registered_window_owner_sources.front();
    for (const auto& source : directory) {
        if (!std::isfinite(source.position[0]) || !std::isfinite(source.position[1]))
            throw std::runtime_error("startup graphics 2D coordinate is not finite");
    }
    std::vector<std::vector<StartupGraphicsRowLocation>> matches;
    for (std::size_t overlay = 0; overlay < directory.size(); ++overlay) {
        bool belongs_to_registered_route = false;
        for (auto ancestor = overlay; hierarchy[ancestor].parent_directory_index;) {
            ancestor = *hierarchy[ancestor].parent_directory_index;
            if (ancestor == registered_owner) {
                belongs_to_registered_route = true;
                break;
            }
        }
        if (!belongs_to_registered_route) continue;
        if (directory[overlay].source_type != overlay_type ||
            !children_are_valid(hierarchy, overlay))
            continue;
        const auto& children = hierarchy[overlay].children_in_directory_order;
        if (children.empty() ||
            !std::ranges::all_of(children, [&](std::size_t child) {
                return directory[child].source_type == row_type;
            }))
            continue;

        std::vector<StartupGraphicsRowLocation> rows;
        bool valid = true;
        for (const auto owner : children) {
            if (!children_are_valid(hierarchy, owner)) {
                valid = false;
                break;
            }
            std::optional<std::size_t> control;
            std::optional<std::size_t> background;
            for (const auto child : hierarchy[owner].children_in_directory_order) {
                const auto type = directory[child].source_type;
                if (type == control_type) {
                    if (control) { valid = false; break; }
                    control = child;
                } else if (type == picture_type) {
                    if (background) { valid = false; break; }
                    background = child;
                } else if (type != text_type) {
                    valid = false;
                    break;
                }
            }
            if (!valid || !control || !background ||
                !children_are_valid(hierarchy, *control)) {
                valid = false;
                break;
            }
            StartupGraphicsRowLocation row{.owner_directory_index = owner,
                                            .background_directory_index = *background,
                                            .chrome_directory_indexes = {}};
            const auto& chrome_children =
                hierarchy[*control].children_in_directory_order;
            if (chrome_children.empty() ||
                !std::ranges::all_of(chrome_children, [&](std::size_t child) {
                    return directory[child].source_type == picture_type;
                })) {
                valid = false;
                break;
            }
            row.chrome_directory_indexes = chrome_children;
            rows.push_back(std::move(row));
            if (rows.size() > maximum_startup_graphics_rows)
                throw std::runtime_error(
                    "startup graphics row count exceeds its structural bound");
        }
        if (valid) {
            std::ranges::sort(rows, [&](const auto& left, const auto& right) {
                const auto left_y = directory[left.owner_directory_index].position[1];
                const auto right_y = directory[right.owner_directory_index].position[1];
                if (left_y != right_y) return left_y < right_y;
                return left.owner_directory_index < right.owner_directory_index;
            });
            matches.push_back(std::move(rows));
        }
    }
    if (matches.size() != 1)
        throw std::runtime_error(
            "registered startup graphics route has no unique structural overlay");
    return std::move(matches.front());
}

StartupGraphicsComposition StartupGraphicsComposition::build(
    const GmsImage& image,
    std::span<const std::byte> primitive_allocation,
    const TextureCatalog& textures
) {
    std::vector<std::size_t> registered_window_owner_sources;
    for (std::size_t source_index = 0;
         source_index < image.directory().size(); ++source_index) {
        const auto& source = image.directory()[source_index];
        if (source.source_type != window_type) continue;
        for (std::size_t attachment_index = 0;
             attachment_index < source.attachments.size(); ++attachment_index) {
            if (equal_ascii_case_insensitive(
                    image.attachment_identifier(source_index, attachment_index),
                    graphics_menu_component_identifier))
                registered_window_owner_sources.push_back(source_index);
        }
    }
    const auto locations = locate(image.directory(), image.hierarchy(),
                                  registered_window_owner_sources);
    std::vector<StartupGraphicsRowComposition> built_rows;
    built_rows.reserve(locations.size());
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
        std::vector<std::pair<StartupGraphicsCompositionRole, std::size_t>> pictures;
        pictures.reserve(location.chrome_directory_indexes.size() + 1U);
        pictures.emplace_back(StartupGraphicsCompositionRole::row_background,
                              location.background_directory_index);
        for (const auto chrome : location.chrome_directory_indexes)
            pictures.emplace_back(StartupGraphicsCompositionRole::row_chrome, chrome);
        for (const auto& [role, directory_index] : pictures) {
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
            if (picture.draw_groups().empty() ||
                picture.texture_resources().size() != picture.draw_groups().size()) {
                throw std::runtime_error("startup graphics picture composition shape mismatch");
            }
            std::size_t previous_descriptor_end = 0;
            for (std::size_t group = 0; group < picture.draw_groups().size(); ++group) {
                const auto& span = picture.draw_groups()[group];
                if (span.descriptor_span_count == 0 ||
                    span.first_descriptor_index < previous_descriptor_end)
                    throw std::runtime_error(
                        "startup graphics picture descriptor spans are invalid");
                previous_descriptor_end =
                    span.first_descriptor_index + span.descriptor_span_count;
            }
            const auto bindings = PictureTextureBindings::build(
                picture.texture_resources(), textures, true);
            auto [chain, transforms] = make_chain(directory_index);
            row.pictures.push_back({
                .role = role,
                .directory_index = directory_index,
                .construction_chain = std::move(chain),
                .transform_chain = std::move(transforms),
                .picture_reference = source.picture_asset_reference,
                .authored_state_mask = authored_state_mask,
                .base_render_property = source.base_render_property,
                .authored_alpha = source.authored_alpha,
                .alignment_enum = source.alignment_enum,
                .extension_control = source.extension_control,
                .draw_plan = PictureDrawPlan::build(picture, bindings)});
        }
        built_rows.push_back(std::move(row));
    }
    return from_rows(std::move(built_rows));
}

StartupGraphicsComposition StartupGraphicsComposition::from_rows(
    std::vector<StartupGraphicsRowComposition> rows
) {
    StartupGraphicsComposition result;
    result.rows_ = std::move(rows);
    if (result.rows_.empty() ||
        result.rows_.size() > maximum_startup_graphics_rows)
        throw std::runtime_error(
            "startup graphics composition has no rows or exceeds its row bound");
    std::unordered_set<std::size_t> row_identities;
    std::unordered_set<std::size_t> picture_identities;
    std::size_t picture_count = 0;
    std::size_t draw_count = 0;
    for (std::size_t row_index = 0; row_index < result.rows_.size(); ++row_index) {
        const auto& row = result.rows_[row_index];
        if (!row_identities.insert(row.owner_directory_index).second)
            throw std::runtime_error("startup graphics row identity is duplicated");
        if (row.pictures.size() < 2U ||
            row.pictures.front().role != StartupGraphicsCompositionRole::row_background ||
            !std::ranges::all_of(
                row.pictures | std::views::drop(1), [](const auto& picture) {
                    return picture.role == StartupGraphicsCompositionRole::row_chrome;
                }))
            throw std::runtime_error("startup graphics row role bindings are invalid");
        if (row.pictures.size() > maximum_startup_graphics_pictures - picture_count)
            throw std::runtime_error(
                "startup graphics picture count exceeds its structural bound");
        picture_count += row.pictures.size();
        for (const auto& picture : row.pictures) {
            if (picture.authored_state_mask == 0 ||
                (picture.authored_state_mask &
                 static_cast<std::uint8_t>(picture.authored_state_mask - 1U)) != 0 ||
                (picture.authored_state_mask & allowed_startup_states) == 0 ||
                picture.draw_plan.groups().empty())
                throw std::runtime_error(
                    "startup graphics picture state or draw data is invalid");
            for (const auto& group : picture.draw_plan.groups()) {
                if (group.quads.size() > maximum_startup_graphics_draws - draw_count)
                    throw std::runtime_error(
                        "startup graphics draw count exceeds its structural bound");
                draw_count += group.quads.size();
            }
            if (picture.alignment_enum > 15U ||
                (picture.extension_control.has_value() &&
                 *picture.extension_control > 16U))
                throw std::runtime_error(
                    "startup graphics picture authored controls are out of range");
            if (!picture_identities.insert(picture.directory_index).second)
                throw std::runtime_error("startup graphics picture identity is duplicated");
            std::unordered_set<std::size_t> descriptor_identities;
            for (std::size_t group_index = 0;
                 group_index < picture.draw_plan.groups().size(); ++group_index) {
                const auto& group = picture.draw_plan.groups()[group_index];
                if (group.quads.empty())
                    throw std::runtime_error("startup graphics draw group is empty");
                for (const auto& quad : group.quads)
                    if (!descriptor_identities.insert(quad.descriptor_index).second)
                        throw std::runtime_error(
                            "startup graphics descriptor identity is duplicated");
            }
        }
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
        const auto& row = result.rows_[i];
        const auto multiplicity = static_cast<std::size_t>(std::ranges::count_if(
            result.rows_, [&](const auto& other) { return other.slot_y == row.slot_y; }));
        const auto ordinal = static_cast<std::size_t>(std::ranges::count_if(
            result.rows_ | std::views::take(i),
            [&](const auto& earlier) { return earlier.slot_y == row.slot_y; }));
        if (!std::isfinite(row.slot_y) ||
            row.same_slot_multiplicity != multiplicity ||
            row.same_slot_ordinal != ordinal)
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
    return result;
}

StartupGraphicsVisibility StartupGraphicsComposition::visible_pictures(
    std::uint8_t requested_state
) const {
    StartupGraphicsVisibility result;
    result.requested_state = requested_state;
    result.effective_state =
        (requested_state & allowed_startup_states) != 0
            ? requested_state
            : resting_state;
    std::size_t picture_count = 0;
    for (const auto& row : rows_) picture_count += row.pictures.size();
    result.pictures.reserve(picture_count);
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
                std::accumulate(picture.draw_plan.groups().begin(),
                                picture.draw_plan.groups().end(), std::size_t{0},
                                [](std::size_t total, const auto& group) {
                                    return total + group.quads.size();
                                }),
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
    std::size_t draw_count = 0;
    for (const auto& visible : visibility.pictures) {
        if (visible.draw_emission_count > maximum_startup_graphics_draws - draw_count)
            throw std::runtime_error(
                "startup graphics traversal exceeds its draw bound");
        draw_count += visible.draw_emission_count;
    }
    result.draw_emissions.reserve(draw_count);

    std::vector<std::size_t> live_rows;
    live_rows.reserve(rows_.size());
    for (std::size_t row_index = 0; row_index < rows_.size(); ++row_index)
        if (!rows_[row_index].authored_hidden) live_rows.push_back(row_index);
    std::ranges::sort(live_rows, std::greater{}, [&](std::size_t row_index) {
        return rows_[row_index].owner_directory_index;
    });

    for (const auto row_index : live_rows) {
        const auto& row = rows_[row_index];
        std::vector<std::size_t> picture_order;
        picture_order.reserve(row.pictures.size());
        for (std::size_t picture_index = 1;
             picture_index < row.pictures.size(); ++picture_index)
            picture_order.push_back(picture_index);
        picture_order.push_back(0);
        for (const auto picture_index : picture_order) {
            const auto& picture = row.pictures[picture_index];
            const auto persistent =
                (picture.authored_state_mask & persistent_state_mask) != 0;
            if (!persistent &&
                (picture.authored_state_mask & result.effective_state) == 0)
                continue;
            const auto first_draw = result.draw_emissions.size();
            const auto draw_count = std::accumulate(
                picture.draw_plan.groups().begin(), picture.draw_plan.groups().end(),
                std::size_t{0}, [](std::size_t total, const auto& group) {
                    return total + group.quads.size();
                });
            result.pictures.push_back({{
                row_index,
                picture_index,
                row.owner_directory_index,
                picture.directory_index,
                picture.role,
                draw_count,
            }, first_draw});
            for (std::size_t group_index = 0;
                 group_index < picture.draw_plan.groups().size(); ++group_index) {
                const auto& group = picture.draw_plan.groups()[group_index];
                for (const auto& quad : group.quads)
                    result.draw_emissions.push_back({
                        row_index, picture_index, picture.directory_index,
                        group_index, quad.descriptor_index});
            }
        }
    }
    return result;
}

}  // namespace off::data
