#pragma once

#include "off/data/picture_texture_binding.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace off::graphics {

// One explicit runtime owner over scene-owned mutable storage. Shared descriptor
// spans/material pointers remain shared; this class does not clone PRM identities.
// Caller retains storage lifetime and prevents concurrent mutation. Immutable
// descriptor encodings remain authored bytes, not serialized runtime state.
class PictureColorState final {
public:
    PictureColorState(std::uint8_t owner_alpha, std::uint32_t owner_color,
                      std::uint32_t runtime_material,
                      std::span<data::PictureResourceDescriptor> descriptors,
                      std::span<std::uint32_t* const> paired_materials);
    PictureColorState(const PictureColorState&) = delete;
    PictureColorState& operator=(const PictureColorState&) = delete;
    PictureColorState(PictureColorState&&) = delete;
    PictureColorState& operator=(PictureColorState&&) = delete;

    // Explicit ordinary material refresh, not a complete picture-load lifecycle.
    void refresh_material(std::uint32_t base_property) noexcept;
    // Full argument comparison with 255 precedes low-byte storage. Writes all
    // descriptor colors, even for unchanged alpha; does not dirty transforms.
    void set_alpha(std::uint32_t argument) noexcept;
    [[nodiscard]] std::uint8_t alpha() const noexcept { return alpha_; }
    [[nodiscard]] std::uint32_t color() const noexcept { return color_; }
    [[nodiscard]] std::uint32_t material() const noexcept { return material_; }

    // Fresh render snapshot from CURRENT colors. Earlier snapshots remain copies.
    [[nodiscard]] data::PictureDrawPlan draw_plan(
        std::span<const data::PictureDrawGroup> groups,
        std::span<const data::PictureTextureBinding> bindings) const;

private:
    void write_material(std::uint32_t value) noexcept;
    std::uint8_t alpha_;
    std::uint32_t color_;
    std::uint32_t material_;
    std::span<data::PictureResourceDescriptor> descriptors_;
    std::vector<std::uint32_t*> paired_materials_;
};

} // namespace off::graphics
