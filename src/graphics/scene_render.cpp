#include "off/graphics/scene_render.hpp"

#include "off/data/packed_resource.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
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
constexpr std::size_t maximum_scene_archives = 4096;
constexpr std::size_t maximum_scene_directory_entries = 1024;
constexpr std::array required_scene_extensions{".prm", ".tex", ".gms", ".rmc",
                                               ".rmi"};
constexpr std::uint32_t zgroup_source_type = 0x00100001U;
constexpr std::uint32_t zroom_source_type = 0x00100021U;

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
          "scene archive contains duplicate scene-resource members");
    }
    match = &entry;
  }
  if (match == nullptr) {
    throw std::runtime_error(
        "scene archive does not contain the required scene resources");
  }
  return *match;
}

[[nodiscard]] bool is_complete_scene_archive(const data::ZipArchive &archive) {
  std::array<std::size_t, required_scene_extensions.size()> counts{};
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot == std::string::npos)
      continue;
    const auto extension = lowercase(entry.name.substr(dot));
    const auto found = std::ranges::find(required_scene_extensions, extension);
    if (found != required_scene_extensions.end()) {
      ++counts[static_cast<std::size_t>(
          std::distance(required_scene_extensions.begin(), found))];
    }
  }
  return std::ranges::all_of(counts,
                             [](std::size_t count) { return count != 0; });
}

[[nodiscard]] SceneRenderAsset
build_scene_render_asset_from_archive(const data::ZipArchive &archive) {
  const data::ZipEntry *animation{};
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot == std::string::npos || lowercase(entry.name.substr(dot)) != ".anm")
      continue;
    if (animation)
      throw std::runtime_error("scene archive has invalid animation resource");
    animation = &entry;
  }
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
  auto result =
      build_scene_render_asset(primitives.entries(), textures.images(),
                               objects.directory(), objects.hierarchy(), maps);
  if (animation)
    result.animation = data::AnimationImage::parse(archive.read(*animation));
  validate_scene_render_asset(result);
  return result;
}

[[nodiscard]] std::vector<SceneGeometryResolution>
expand_container_resolution(const SceneGeometryResolution &root,
                            std::span<const data::PrimitiveEntry> primitives,
                            std::span<const data::GmsDirectoryEntry> sources,
                            std::span<const data::GmsHierarchyNode> hierarchy) {
  if (root.status != SceneGeometryStatus::source_without_primitive ||
      !root.source_directory_index.has_value() || hierarchy.empty())
    return {};
  const auto root_index = *root.source_directory_index;
  if (root_index >= sources.size() || root_index >= hierarchy.size())
    throw std::runtime_error("scene container hierarchy is inconsistent");
  const auto type = sources[root_index].source_type;
  if ((type != zgroup_source_type && type != zroom_source_type) ||
      sources[root_index].class_data_value != 0U)
    return {};
  std::unordered_map<std::uint32_t, std::size_t> primitive_by_reference;
  for (std::size_t index = 0; index < primitives.size(); ++index) {
    if (!primitive_by_reference.emplace(primitives[index].packed_index, index)
             .second)
      throw std::runtime_error(
          "scene geometry reference resolves to duplicate PRM indexes");
  }
  std::vector<SceneGeometryResolution> result;
  std::function<void(std::size_t)> visit = [&](std::size_t index) {
    if (index >= hierarchy.size() || index >= sources.size() ||
        hierarchy[index].directory_index != index)
      throw std::runtime_error("scene container hierarchy is inconsistent");
    const auto &source = sources[index];
    if (source.primitive_reference.has_value()) {
      auto child = root;
      child.source_directory_index = index;
      child.source_local_slot_index = source.local_slot_index;
      child.primitive_reference = source.primitive_reference;
      const auto found =
          primitive_by_reference.find(*source.primitive_reference);
      if (found == primitive_by_reference.end()) {
        child.status = SceneGeometryStatus::missing_primitive;
        child.primitive_entry_index.reset();
      } else {
        child.primitive_entry_index = found->second;
        child.status = primitives[found->second].flagged_reference
                           ? SceneGeometryStatus::unresolved_primitive_alias
                           : SceneGeometryStatus::local_primitive;
      }
      result.push_back(std::move(child));
    }
    for (const auto child : hierarchy[index].children_in_directory_order)
      visit(child);
  };
  for (const auto child : hierarchy[root_index].children_in_directory_order)
    visit(child);
  return result;
}

} // namespace

SceneRenderResolutionSummary
summarize_scene_render_resolutions(const SceneRenderAsset &asset) noexcept {
  SceneRenderResolutionSummary result;
  for (const auto &resolution : asset.resolutions) {
    switch (resolution.geometry.status) {
    case SceneGeometryStatus::local_primitive:
      ++result.local_primitive;
      break;
    case SceneGeometryStatus::no_local_source:
      ++result.no_local_source;
      break;
    case SceneGeometryStatus::source_without_primitive:
      ++result.source_without_primitive;
      break;
    case SceneGeometryStatus::missing_primitive:
      ++result.missing_primitive;
      break;
    case SceneGeometryStatus::unresolved_primitive_alias:
      ++result.unresolved_primitive_alias;
      break;
    }
  }
  return result;
}

void validate_scene_render_asset(const SceneRenderAsset &asset) {
  if (asset.animation &&
      (asset.animation->header().byte_size < 20U ||
       asset.animation->header().reference_table_count < 1U ||
       asset.animation->header().reference_table_count > 6U ||
       asset.animation->header().format_value != 10U ||
       asset.animation->reference_tables().size() !=
           asset.animation->header().reference_table_count ||
       asset.animation->descriptors().empty() ||
       asset.animation->sections().size() != 8U)) {
    throw std::invalid_argument("scene render animation metadata is invalid");
  }
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
  return build_scene_render_asset(primitives, textures, object_sources, {},
                                  maps);
}

SceneRenderAsset build_scene_render_asset(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::GmsHierarchyNode> hierarchy,
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
    std::vector<SceneGeometryResolution> expanded;
    for (const auto &geometry : resolutions) {
      expanded.push_back(geometry);
      auto descendants = expand_container_resolution(geometry, primitives,
                                                     object_sources, hierarchy);
      expanded.insert(expanded.end(),
                      std::make_move_iterator(descendants.begin()),
                      std::make_move_iterator(descendants.end()));
    }
    for (const auto &geometry : expanded) {
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
load_scene_render_asset(const std::filesystem::path &archive_path) {
  return build_scene_render_asset_from_archive(
      data::ZipArchive::open(archive_path));
}

SceneRenderAsset
load_startup_scene_render_asset(const std::filesystem::path &install_root) {
  return load_scene_render_asset(install_root / "Scenes" / "FF-StartUp.ZIP");
}

SceneRenderAsset
load_diagnostic_scene_render_asset(const std::filesystem::path &install_root) {
  const auto scenes_root = install_root / "Scenes";
  std::vector<std::filesystem::path> candidates;
  std::error_code error;
  std::filesystem::recursive_directory_iterator iterator{
      scenes_root, std::filesystem::directory_options::none, error};
  const std::filesystem::recursive_directory_iterator end;
  if (error)
    throw std::runtime_error("could not enumerate the scene directory");
  std::size_t entries = 0;
  for (; iterator != end; iterator.increment(error)) {
    if (error)
      throw std::runtime_error("could not enumerate the scene directory");
    if (++entries > maximum_scene_directory_entries)
      throw std::runtime_error(
          "scene archive directory exceeds the safety entry limit");
    const auto status = iterator->symlink_status(error);
    if (error)
      throw std::runtime_error("could not inspect a scene directory entry");
    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status) ||
        lowercase(iterator->path().extension().string()) != ".zip") {
      continue;
    }
    if (candidates.size() == maximum_scene_archives) {
      throw std::runtime_error("scene archive count exceeds selection budget");
    }
    candidates.push_back(iterator->path());
  }
  if (error)
    throw std::runtime_error("could not enumerate the scene directory");
  const auto ordering_key = [&](const std::filesystem::path &path) {
    return lowercase(path.lexically_relative(scenes_root).generic_string());
  };
  std::ranges::sort(candidates, [&](const auto &left, const auto &right) {
    const auto left_key = ordering_key(left);
    const auto right_key = ordering_key(right);
    return left_key == right_key
               ? left.generic_string() < right.generic_string()
               : left_key < right_key;
  });
  for (const auto &path : candidates) {
    const auto archive = data::ZipArchive::open(path);
    if (!is_complete_scene_archive(archive))
      continue;
    auto asset = build_scene_render_asset_from_archive(archive);
    if (!asset.instances.empty())
      return asset;
  }
  throw std::runtime_error(
      "installation contains no renderable direct-local scene archive");
}

SceneRenderAsset load_owned_diagnostic_scene_render_asset(
    const std::filesystem::path &install_root,
    const std::filesystem::path &relative_archive_path) {
  if (relative_archive_path.empty() || relative_archive_path.is_absolute() ||
      lowercase(relative_archive_path.extension().string()) != ".zip")
    throw std::runtime_error("diagnostic scene archive path is invalid");
  for (const auto &component : relative_archive_path) {
    if (component == "..")
      throw std::runtime_error("diagnostic scene archive path is invalid");
  }
  const auto archive_path = install_root / "Scenes" / relative_archive_path;
  std::error_code error;
  const auto status = std::filesystem::symlink_status(archive_path, error);
  if (error || std::filesystem::is_symlink(status) ||
      !std::filesystem::is_regular_file(status))
    throw std::runtime_error("diagnostic scene archive is unavailable");
  return load_scene_render_asset(archive_path);
}

} // namespace off::graphics
