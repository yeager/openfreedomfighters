#include "off/graphics/picture_color_state.hpp"
#include "off/graphics/picture_material_state.hpp"

#include <stdexcept>

namespace off::graphics {

PictureColorState::PictureColorState(std::uint8_t owner_alpha, std::uint32_t owner_color,
                                     std::uint32_t runtime_material,
                                     std::span<data::PictureResourceDescriptor> descriptors,
                                     std::span<std::uint32_t* const> paired_materials)
    : alpha_(owner_alpha), color_(owner_color), material_(runtime_material),
      descriptors_(descriptors), paired_materials_(paired_materials.begin(), paired_materials.end()) {
    for (const auto pointer : paired_materials_)
        if (!pointer) throw std::runtime_error("picture paired material storage must be present");
}

void PictureColorState::write_material(std::uint32_t value) noexcept {
    material_ = value;
    for (const auto pointer : paired_materials_) *pointer = value;
}

void PictureColorState::refresh_material(std::uint32_t base_property) noexcept {
    write_material(map_base_picture_material_property(base_property));
}

void PictureColorState::set_alpha(std::uint32_t argument) noexcept {
    const auto transition = update_picture_alpha_material(material_, argument);
    if (transition.material_changed) write_material(transition.material_word);
    alpha_ = static_cast<std::uint8_t>(argument);
    color_ = (color_ & 0x00ffffffU) | (static_cast<std::uint32_t>(alpha_) << 24U);
    for (auto& descriptor : descriptors_) descriptor.modulation_color = color_;
}

data::PictureDrawPlan PictureColorState::draw_plan(
    std::span<const data::PictureDrawGroup> groups,
    std::span<const data::PictureTextureBinding> bindings) const {
    return data::PictureDrawPlan::build(descriptors_, groups, bindings);
}

} // namespace off::graphics
