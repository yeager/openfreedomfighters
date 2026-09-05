#include "off/data/picture_texture_binding.hpp"

#include <array>
#include <optional>
#include <stdexcept>

namespace off::data {

PictureTextureBindings PictureTextureBindings::build(
    std::span<const PictureTextureResource> resources,
    const TextureCatalog& catalog,
    bool require_upper_bank
) {
    std::array<std::optional<std::size_t>, texture_manager_slot_count> images{};
    for (std::size_t index = 0; index < catalog.images().size(); ++index) {
        const auto id = catalog.images()[index].id;
        if (id >= texture_manager_slot_count || images[id].has_value()) {
            throw std::runtime_error("duplicate or invalid texture image ID");
        }
        images[id] = index;
    }
    std::array<bool, texture_manager_slot_count> sequences{};
    for (const auto& sequence : catalog.sequences()) {
        if (sequence.id >= texture_manager_slot_count || sequences[sequence.id]) {
            throw std::runtime_error("duplicate or invalid texture sequence ID");
        }
        sequences[sequence.id] = true;
    }

    std::array<std::optional<TextureManagerKeyBank>, texture_manager_slot_count>
        observed_banks{};
    PictureTextureBindings result;
    result.entries_.reserve(resources.size());
    for (const auto& resource : resources) {
        const auto key = resource.manager_key;
        if (key >= texture_manager_slot_count * 2U) {
            throw std::runtime_error("picture texture-manager key is out of range");
        }
        const auto bank = key < texture_manager_slot_count
            ? TextureManagerKeyBank::direct
            : TextureManagerKeyBank::upper;
        if (require_upper_bank && bank != TextureManagerKeyBank::upper) {
            throw std::runtime_error("picture texture-manager key uses an unsupported bank");
        }
        const auto id = static_cast<std::uint16_t>(
            key % texture_manager_slot_count);
        if (observed_banks[id].has_value() && *observed_banks[id] != bank) {
            throw std::runtime_error("picture texture-manager keys alias across banks");
        }
        observed_banks[id] = bank;
        if (sequences[id]) {
            throw std::runtime_error(
                "picture texture sequence selection is not supported");
        }
        if (!images[id].has_value()) {
            throw std::runtime_error("picture texture-manager key has no image");
        }
        result.entries_.push_back({
            .prm_offset = resource.prm_offset,
            .manager_key = key,
            .texture_id = id,
            .image_index = *images[id],
            .bank = bank,
        });
    }
    return result;
}

PictureDrawPlan PictureDrawPlan::build(
    const PictureResource& picture,
    const PictureTextureBindings& textures
) {
    return build(picture.descriptors(), picture.draw_groups(), textures.entries());
}

PictureDrawPlan PictureDrawPlan::build(
    std::span<const PictureResourceDescriptor> descriptors,
    std::span<const PictureDrawGroup> groups,
    std::span<const PictureTextureBinding> textures
) {
    if (groups.size() != textures.size()) {
        throw std::runtime_error(
            "picture draw-group and texture-binding counts do not match"
        );
    }

    PictureDrawPlan result;
    result.groups_.reserve(groups.size());
    for (std::size_t group_index = 0;
         group_index < groups.size(); ++group_index) {
        const auto& group = groups[group_index];
        if (group.first_descriptor_index > descriptors.size() ||
            group.descriptor_span_count >
                descriptors.size() - group.first_descriptor_index) {
            throw std::runtime_error(
                "picture draw-group descriptor span is out of range"
            );
        }
        BoundPictureDrawGroup bound{.texture = textures[group_index], .quads = {}};
        bound.quads.reserve(group.descriptor_span_count);
        for (std::size_t relative = 0;
             relative < group.descriptor_span_count; ++relative) {
            const auto descriptor_index = group.first_descriptor_index + relative;
            const auto& descriptor = descriptors[descriptor_index];
            const auto half_width = descriptor.horizontal_edge_span * 0.5F;
            const auto half_height = descriptor.vertical_edge_span * 0.5F;
            bound.quads.push_back({
                .local_x_min = descriptor.local_center_x - half_width,
                .local_x_max = descriptor.local_center_x + half_width,
                .local_y_min = descriptor.local_center_y - half_height,
                .local_y_max = descriptor.local_center_y + half_height,
                .local_z = descriptor.local_z,
                .u_min = descriptor.u_min,
                .u_max = descriptor.u_max,
                .v_max = descriptor.v_max,
                .v_min = descriptor.v_min,
                .modulation_color = descriptor.modulation_color,
                .descriptor_index = descriptor_index,
                .local_center_x = descriptor.local_center_x,
                .local_center_y = descriptor.local_center_y,
                .horizontal_edge_span = descriptor.horizontal_edge_span,
                .vertical_edge_span = descriptor.vertical_edge_span,
            });
        }
        result.groups_.push_back(std::move(bound));
    }
    return result;
}

}  // namespace off::data
