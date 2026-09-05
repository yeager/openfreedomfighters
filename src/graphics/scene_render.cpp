#include "off/graphics/scene_render.hpp"

#include "off/data/packed_resource.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
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

[[nodiscard]] bool finite(const auto &values) {
  return std::ranges::all_of(values,
                             [](float value) { return std::isfinite(value); });
}

[[nodiscard]] std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

[[nodiscard]] const data::ZipEntry &
unique_member_with_extension(const data::ZipArchive &archive,
                             const char *extension) {
  const data::ZipEntry *match = nullptr;
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot == std::string::npos ||
        lowercase(entry.name.substr(dot)) != extension) {
      continue;
    }
    if (match != nullptr) {
      throw std::runtime_error(
          "startup archive contains duplicate scene-resource members");
    }
    match = &entry;
  }
  if (match == nullptr) {
    throw std::runtime_error(
        "startup archive does not contain the required scene resources");
  }
  return *match;
}

} // namespace

void validate_scene_render_asset(const SceneRenderAsset &asset) {
  if (asset.resolutions.size() > maximum_scene_instances ||
      asset.instances.size() > maximum_scene_instances) {
    throw std::invalid_argument("scene render asset exceeds instance budget");
  }

  std::size_t total_vertices = 0;
  std::size_t total_indices = 0;
  std::size_t total_draws = 0;
  std::size_t total_rgba_bytes = 0;
  for (const auto &texture : asset.textures) {
    if (texture.mip_zero.width == 0 || texture.mip_zero.height == 0 ||
        texture.mip_zero.width > std::numeric_limits<std::size_t>::max() / 4U ||
        texture.mip_zero.height >
            std::numeric_limits<std::size_t>::max() /
                (static_cast<std::size_t>(texture.mip_zero.width) * 4U)) {
      throw std::invalid_argument(
          "scene render texture has invalid dimensions");
    }
    const auto expected_bytes =
        static_cast<std::size_t>(texture.mip_zero.width) *
        texture.mip_zero.height * 4U;
    if (texture.mip_zero.pixels.size() != expected_bytes) {
      throw std::invalid_argument(
          "scene render texture has inconsistent RGBA storage");
    }
    add_bounded(total_rgba_bytes, expected_bytes, maximum_scene_rgba_bytes,
                "scene render asset exceeds decoded texture budget");
  }

  for (const auto &mesh : asset.meshes) {
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.draws.empty() ||
        (mesh.texture_index.has_value() &&
         *mesh.texture_index >= asset.textures.size())) {
      throw std::invalid_argument("scene render mesh has incomplete resources");
    }
    add_bounded(total_vertices, mesh.vertices.size(), maximum_scene_vertices,
                "scene render asset exceeds vertex budget");
    add_bounded(total_indices, mesh.indices.size(), maximum_scene_indices,
                "scene render asset exceeds index budget");
    add_bounded(total_draws, mesh.draws.size(), maximum_scene_draws,
                "scene render asset exceeds draw budget");
    for (const auto &vertex : mesh.vertices) {
      if (!finite(vertex.position) || !finite(vertex.normal) ||
          !finite(vertex.texture_coordinates)) {
        throw std::invalid_argument(
            "scene render mesh has a non-finite vertex attribute");
      }
    }
    if (std::ranges::any_of(mesh.indices, [&](const auto index) {
          return index >= mesh.vertices.size();
        })) {
      throw std::invalid_argument(
          "scene render mesh contains an invalid vertex index");
    }
    std::size_t expected_first_index = 0;
    for (const auto &draw : mesh.draws) {
      const auto minimum_count =
          mesh.topology == PrimitiveTopology::triangle_strip ? 3U : 2U;
      if (draw.first_index != expected_first_index ||
          draw.index_count < minimum_count ||
          (mesh.topology == PrimitiveTopology::line_list &&
           draw.index_count != 2U) ||
          draw.first_index > mesh.indices.size() ||
          draw.index_count > mesh.indices.size() - draw.first_index) {
        throw std::invalid_argument(
            "scene render mesh has an invalid draw range");
      }
      expected_first_index += draw.index_count;
    }
    if (expected_first_index != mesh.indices.size() ||
        mesh.minimum_vertex_alpha > mesh.maximum_vertex_alpha) {
      throw std::invalid_argument("scene render mesh metadata is inconsistent");
    }
  }

  for (const auto &resolution : asset.resolutions) {
    if (resolution.map_layer_index >= maximum_map_layers) {
      throw std::invalid_argument(
          "scene render resolution has an invalid map layer");
    }
  }
  for (const auto &instance : asset.instances) {
    if (instance.resolution_index >= asset.resolutions.size() ||
        instance.mesh_index >= asset.meshes.size() ||
        instance.map_layer_index >= maximum_map_layers ||
        !finite(instance.source_basis) || !finite(instance.source_position) ||
        !finite(instance.map_orientation) || !finite(instance.map_position) ||
        !finite(instance.map_auxiliary_position) ||
        !finite(instance.map_extents)) {
      throw std::invalid_argument(
          "scene render instance has invalid references or values");
    }
    const auto &resolution = asset.resolutions[instance.resolution_index];
    const auto &mesh = asset.meshes[instance.mesh_index];
    if (resolution.geometry.status != SceneGeometryStatus::local_primitive ||
        resolution.map_kind != instance.map_kind ||
        resolution.map_layer_index != instance.map_layer_index ||
        resolution.geometry.role != instance.role ||
        resolution.geometry.map_entry_index != instance.map_entry_index ||
        resolution.geometry.map_descriptor_offset !=
            instance.map_descriptor_offset ||
        resolution.geometry.geometry_reference != instance.geometry_reference ||
        !resolution.geometry.source_directory_index.has_value() ||
        *resolution.geometry.source_directory_index !=
            instance.source_directory_index ||
        !resolution.geometry.source_local_slot_index.has_value() ||
        *resolution.geometry.source_local_slot_index !=
            instance.source_local_slot_index ||
        !resolution.geometry.primitive_reference.has_value() ||
        *resolution.geometry.primitive_reference !=
            mesh.primitive_packed_index ||
        !resolution.geometry.primitive_entry_index.has_value() ||
        *resolution.geometry.primitive_entry_index !=
            mesh.primitive_entry_index) {
      throw std::invalid_argument(
          "scene render instance provenance is inconsistent");
    }
  }
}

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
  validate_scene_render_asset(result);
  return result;
}

SceneRenderAsset
load_startup_scene_render_asset(const std::filesystem::path &install_root) {
  const auto archive =
      data::ZipArchive::open(install_root / "Scenes" / "FF-StartUp.ZIP");
  const auto primitive_bytes =
      archive.read(unique_member_with_extension(archive, ".prm"));
  const auto texture_bytes =
      archive.read(unique_member_with_extension(archive, ".tex"));
  const auto object_bytes =
      archive.read(unique_member_with_extension(archive, ".gms"));
  const auto rmc_bytes =
      archive.read(unique_member_with_extension(archive, ".rmc"));
  const auto rmi_bytes =
      archive.read(unique_member_with_extension(archive, ".rmi"));

  const auto primitives = data::PrimitiveCatalog::parse(primitive_bytes);
  const auto textures = data::TextureCatalog::parse(texture_bytes);
  const auto objects =
      data::GmsImage::parse(data::PackedResource::parse(object_bytes));
  const auto rmc = data::RenderMap::parse(rmc_bytes);
  const auto rmi = data::RenderMap::parse(rmi_bytes);
  const std::array maps{
      SceneRenderMapView{.kind = SceneRenderMapKind::rmc,
                         .entries = rmc.entries()},
      SceneRenderMapView{.kind = SceneRenderMapKind::rmi,
                         .entries = rmi.entries()},
  };
  return build_scene_render_asset(primitives.entries(), textures.images(),
                                  objects.directory(), maps);
}

} // namespace off::graphics
