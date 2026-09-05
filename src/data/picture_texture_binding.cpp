#include "off/data/picture_texture_binding.hpp"

#include <array>
#include <optional>
#include <stdexcept>

namespace off::data {

PictureTextureBindings PictureTextureBindings::build(
    std::span<const PictureFrameTextureResource> resources,
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

}  // namespace off::data
