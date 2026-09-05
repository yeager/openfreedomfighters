#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/primitive_catalog.hpp"
#include "off/data/render_map.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/graphics/render_assets.hpp"
#include "off/graphics/texture_decode.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

struct RenderMapInstance {
  std::size_t map_entry_index{0};
  std::uint32_t map_descriptor_offset{0};
  std::uint32_t geometry_reference{0};
  std::array<float, 9> orientation{};
  std::array<float, 3> position{};
};

struct RenderObjectInstance {
  std::array<float, 9> basis{};
  std::array<float, 3> position{};
  std::uint32_t source_type{0};
  std::size_t directory_index{0};
  std::uint32_t local_slot_index{0};
  std::optional<RenderMapInstance> map_instance;
};

enum class SceneGeometryRole : std::uint8_t { primary, secondary };

enum class SceneGeometryStatus : std::uint8_t {
  local_primitive,
  no_local_source,
  source_without_primitive,
  missing_primitive,
  unresolved_primitive_alias,
};

struct SceneGeometryResolution {
  std::size_t map_entry_index{0};
  std::uint32_t map_descriptor_offset{0};
  SceneGeometryRole role{SceneGeometryRole::primary};
  std::uint32_t geometry_reference{0};
  std::uint32_t requested_handle_slot_index{0};
  SceneGeometryStatus status{SceneGeometryStatus::no_local_source};
  std::optional<std::size_t> source_directory_index;
  std::optional<std::uint32_t> source_local_slot_index;
  std::optional<std::uint32_t> primitive_reference;
  std::optional<std::size_t> primitive_entry_index;
};

struct RenderPreviewAsset {
  std::vector<data::PrimitiveVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<PrimitiveDrawRange> draws;
  RgbaImage texture;
  std::array<float, 3> minimum_position{};
  std::array<float, 3> maximum_position{};
  std::uint32_t primitive_packed_index{0};
  std::optional<RenderObjectInstance> object_instance;
};

[[nodiscard]] std::array<float, 3>
transform_render_position(const RenderObjectInstance &instance,
                          const std::array<float, 3> &local_position);

void validate_render_preview(const RenderPreviewAsset &preview);

[[nodiscard]] RenderPreviewAsset
build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                     std::span<const data::TextureImage> textures);

[[nodiscard]] std::vector<SceneGeometryResolution>
resolve_scene_geometry_references(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::RenderMapEntry> map_entries);

[[nodiscard]] RenderPreviewAsset build_first_primary_scene_render_preview(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::RenderMapEntry> map_entries);

[[nodiscard]] RenderPreviewAsset
load_startup_render_preview(const std::filesystem::path &install_root);

} // namespace off::graphics
