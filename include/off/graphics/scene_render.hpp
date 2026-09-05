#pragma once

#include "off/graphics/render_preview.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

enum class SceneRenderMapKind : std::uint8_t { rmc, rmi };

struct SceneRenderMapView {
  SceneRenderMapKind kind{SceneRenderMapKind::rmc};
  std::span<const data::RenderMapEntry> entries;
};

struct SceneRenderResolution {
  SceneRenderMapKind map_kind{SceneRenderMapKind::rmc};
  std::size_t map_layer_index{0};
  SceneGeometryResolution geometry;
};

struct SceneRenderTexture {
  std::size_t texture_image_index{0};
  std::uint32_t texture_id{0};
  RgbaImage mip_zero;
};

struct SceneRenderMesh {
  std::size_t primitive_entry_index{0};
  std::uint32_t primitive_packed_index{0};
  PrimitiveTopology topology{PrimitiveTopology::triangle_strip};
  std::vector<data::PrimitiveVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<PrimitiveDrawRange> draws;
  std::optional<std::size_t> texture_index;
  VertexAlphaClass alpha_class{VertexAlphaClass::opaque};
  std::uint8_t minimum_vertex_alpha{255};
  std::uint8_t maximum_vertex_alpha{255};
};

struct SceneRenderInstance {
  std::size_t resolution_index{0};
  std::size_t mesh_index{0};
  SceneRenderMapKind map_kind{SceneRenderMapKind::rmc};
  std::size_t map_layer_index{0};
  SceneGeometryRole role{SceneGeometryRole::primary};
  std::size_t map_entry_index{0};
  std::uint32_t map_descriptor_offset{0};
  std::uint32_t geometry_reference{0};
  std::size_t source_directory_index{0};
  std::uint32_t source_local_slot_index{0};
  std::uint32_t source_type{0};
  std::array<float, 9> source_basis{};
  std::array<float, 3> source_position{};
  std::uint32_t map_object_kind{0};
  std::array<float, 9> map_orientation{};
  std::array<float, 3> map_position{};
  std::array<float, 3> map_auxiliary_position{};
  std::array<float, 3> map_extents{};
  data::QuantizedBounds map_bounds;
};

struct SceneRenderAsset {
  std::vector<SceneRenderTexture> textures;
  std::vector<SceneRenderMesh> meshes;
  std::vector<SceneRenderInstance> instances;
  std::vector<SceneRenderResolution> resolutions;
};

void validate_scene_render_asset(const SceneRenderAsset &asset);

[[nodiscard]] SceneRenderAsset build_scene_render_asset(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const SceneRenderMapView> maps);

[[nodiscard]] SceneRenderAsset
load_scene_render_asset(const std::filesystem::path &archive_path);

[[nodiscard]] SceneRenderAsset
load_startup_scene_render_asset(const std::filesystem::path &install_root);

[[nodiscard]] SceneRenderAsset
load_diagnostic_scene_render_asset(const std::filesystem::path &install_root);

} // namespace off::graphics
