#include "off/graphics/scene_render.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace off::graphics {
namespace {

constexpr std::size_t maximum_map_layers = 16;
constexpr std::size_t maximum_scene_instances = 131'072;
constexpr std::size_t maximum_scene_vertices = 16'000'000;
constexpr std::size_t maximum_scene_indices = 32'000'000;
constexpr std::size_t maximum_scene_draws = 4'000'000;
constexpr std::size_t maximum_scene_rgba_bytes = 1024U * 1024U * 1024U;

void add_bounded(std::size_t &total, std::size_t value, std::size_t limit,
                 const char *message) {
  if (value > limit - total) {
    throw std::runtime_error(message);
  }
  total += value;
}

} // namespace

SceneRenderAsset build_scene_render_asset(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const SceneRenderMapView> maps) {
  if (maps.size() > maximum_map_layers) {
    throw std::runtime_error("scene render asset has too many map layers");
  }
  const auto bindings = RenderAssetBindings::build(primitives, textures);
  std::unordered_map<std::size_t, const PrimitiveTextureBinding *>
      binding_by_primitive;
  binding_by_primitive.reserve(bindings.primitives().size());
  for (const auto &binding : bindings.primitives()) {
    binding_by_primitive.emplace(binding.primitive_entry_index, &binding);
  }

  SceneRenderAsset result;
  std::unordered_map<std::size_t, std::size_t> mesh_by_primitive;
  std::unordered_map<std::size_t, std::size_t> texture_by_image;
  std::size_t total_vertices = 0;
  std::size_t total_indices = 0;
  std::size_t total_draws = 0;
  std::size_t total_rgba_bytes = 0;

  for (std::size_t map_layer_index = 0; map_layer_index < maps.size();
       ++map_layer_index) {
    const auto &map = maps[map_layer_index];
    const auto resolutions = resolve_scene_geometry_references(
        primitives, object_sources, map.entries);
    for (const auto &geometry : resolutions) {
      if (result.resolutions.size() == maximum_scene_instances) {
        throw std::runtime_error("scene render asset exceeds instance budget");
      }
      const auto resolution_index = result.resolutions.size();
      result.resolutions.push_back({
          .map_kind = map.kind,
          .map_layer_index = map_layer_index,
          .geometry = geometry,
      });
      if (geometry.status != SceneGeometryStatus::local_primitive) {
        continue;
      }
      const auto primitive_index = *geometry.primitive_entry_index;
      const auto binding_lookup = binding_by_primitive.find(primitive_index);
      if (binding_lookup == binding_by_primitive.end()) {
        throw std::runtime_error(
            "local scene primitive has no renderer binding");
      }
      const auto &binding = *binding_lookup->second;
      const auto &primitive = primitives[primitive_index];
      for (const auto index : binding.indices) {
        if (index >= primitive.vertices.size()) {
          throw std::runtime_error(
              "scene render mesh contains an invalid vertex index");
        }
      }

      auto mesh_lookup = mesh_by_primitive.find(primitive_index);
      std::size_t mesh_index;
      if (mesh_lookup == mesh_by_primitive.end()) {
        add_bounded(total_vertices, primitive.vertices.size(),
                    maximum_scene_vertices,
                    "scene render asset exceeds vertex budget");
        add_bounded(total_indices, binding.indices.size(),
                    maximum_scene_indices,
                    "scene render asset exceeds index budget");
        add_bounded(total_draws, binding.draws.size(), maximum_scene_draws,
                    "scene render asset exceeds draw budget");
        const auto finite = [](const auto &values) {
          return std::ranges::all_of(
              values, [](float value) { return std::isfinite(value); });
        };
        for (const auto &vertex : primitive.vertices) {
          if (!finite(vertex.position) || !finite(vertex.normal) ||
              !finite(vertex.texture_coordinates)) {
            throw std::runtime_error(
                "scene render mesh has a non-finite vertex attribute");
          }
        }
        std::optional<std::size_t> texture_index;
        if (binding.texture_image_index.has_value()) {
          const auto image_index = *binding.texture_image_index;
          auto texture_lookup = texture_by_image.find(image_index);
          if (texture_lookup == texture_by_image.end()) {
            auto decoded = decode_texture_mip(textures[image_index], 0);
            add_bounded(total_rgba_bytes, decoded.pixels.size(),
                        maximum_scene_rgba_bytes,
                        "scene render asset exceeds decoded texture budget");
            texture_index = result.textures.size();
            result.textures.push_back({
                .texture_image_index = image_index,
                .texture_id = textures[image_index].id,
                .mip_zero = std::move(decoded),
            });
            texture_by_image.emplace(image_index, *texture_index);
          } else {
            texture_index = texture_lookup->second;
          }
        }
        mesh_index = result.meshes.size();
        result.meshes.push_back({
            .primitive_entry_index = primitive_index,
            .primitive_packed_index = primitive.packed_index,
            .topology = binding.topology,
            .vertices = primitive.vertices,
            .indices = binding.indices,
            .draws = binding.draws,
            .texture_index = texture_index,
            .alpha_class = binding.vertex_alpha_class,
            .minimum_vertex_alpha = binding.minimum_vertex_alpha,
            .maximum_vertex_alpha = binding.maximum_vertex_alpha,
        });
        mesh_by_primitive.emplace(primitive_index, mesh_index);
      } else {
        mesh_index = mesh_lookup->second;
      }

      const auto source_index = *geometry.source_directory_index;
      const auto &source = object_sources[source_index];
      const auto &entry = map.entries[geometry.map_entry_index];
      const auto finite = [](const auto &values) {
        return std::ranges::all_of(
            values, [](float value) { return std::isfinite(value); });
      };
      if (!finite(source.basis) || !finite(source.position) ||
          !finite(entry.object.orientation) || !finite(entry.object.position) ||
          !finite(entry.object.auxiliary_position) ||
          !finite(entry.object.extents)) {
        throw std::runtime_error(
            "scene render instance has a non-finite transform or extent");
      }
      result.instances.push_back({
          .resolution_index = resolution_index,
          .mesh_index = mesh_index,
          .map_kind = map.kind,
          .map_layer_index = map_layer_index,
          .role = geometry.role,
          .map_entry_index = geometry.map_entry_index,
          .map_descriptor_offset = geometry.map_descriptor_offset,
          .geometry_reference = geometry.geometry_reference,
          .source_directory_index = source_index,
          .source_local_slot_index = *geometry.source_local_slot_index,
          .source_type = source.source_type,
          .source_basis = source.basis,
          .source_position = source.position,
          .map_object_kind = entry.object.kind,
          .map_orientation = entry.object.orientation,
          .map_position = entry.object.position,
          .map_auxiliary_position = entry.object.auxiliary_position,
          .map_extents = entry.object.extents,
          .map_bounds = entry.bounds,
      });
    }
  }
  return result;
}

} // namespace off::graphics
