#pragma once

#include "off/data/texture_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace off::graphics {

struct RgbaImage {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> pixels;
};

[[nodiscard]] RgbaImage decode_texture_mip(
    const data::TextureImage& texture,
    std::size_t mip_level
);

}  // namespace off::graphics
