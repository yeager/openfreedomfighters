#pragma once

#include "off/data/primitive_catalog.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/graphics/render_assets.hpp"
#include "off/graphics/texture_decode.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace off::graphics {

struct RenderPreviewAsset {
  std::vector<data::PrimitiveVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<PrimitiveDrawRange> draws;
  RgbaImage texture;
  std::array<float, 3> minimum_position{};
  std::array<float, 3> maximum_position{};
};

[[nodiscard]] RenderPreviewAsset
build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                     std::span<const data::TextureImage> textures);

[[nodiscard]] RenderPreviewAsset
load_startup_render_preview(const std::filesystem::path &install_root);

} // namespace off::graphics
