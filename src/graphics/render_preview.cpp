#include "off/graphics/render_preview.hpp"

#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace off::graphics {
namespace {

class NoScenePreviewError final : public std::runtime_error {
public:
  NoScenePreviewError()
      : std::runtime_error(
            "render map contains no supported local preview geometry") {}
};

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
          "startup archive contains duplicate render-resource members");
    }
    match = &entry;
  }
  if (match == nullptr) {
    throw std::runtime_error(
        "startup archive does not contain the required render resources");
  }
  return *match;
}

[[nodiscard]] bool
has_nondegenerate_triangle(std::span<const data::PrimitiveVertex> vertices,
                           std::span<const std::uint16_t> indices,
                           std::span<const PrimitiveDrawRange> draws) {
  for (const auto &draw : draws) {
    for (std::size_t offset = 2; offset < draw.index_count; ++offset) {
      const auto &a = vertices[indices[draw.first_index + offset - 2]].position;
      const auto &b = vertices[indices[draw.first_index + offset - 1]].position;
      const auto &c = vertices[indices[draw.first_index + offset]].position;
      const std::array ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const std::array ac{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const std::array cross{ab[1] * ac[2] - ab[2] * ac[1],
                             ab[2] * ac[0] - ab[0] * ac[2],
                             ab[0] * ac[1] - ab[1] * ac[0]};
      const auto area_squared =
          cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
      if (area_squared > 1.0e-10F) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] std::optional<RenderPreviewAsset>
try_build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                         std::span<const data::TextureImage> textures) {
  const auto bindings = RenderAssetBindings::build(primitives, textures);
  for (const auto &binding : bindings.primitives()) {
    if (binding.topology != PrimitiveTopology::triangle_strip ||
        !binding.texture_image_index.has_value()) {
      continue;
    }
    const auto &primitive = primitives[binding.primitive_entry_index];
    const auto &texture = textures[*binding.texture_image_index];
    if (std::ranges::any_of(binding.indices, [&](const auto index) {
          return index >= primitive.vertices.size();
        })) {
      throw std::runtime_error(
          "render preview contains an invalid vertex index");
    }
    if (texture.mips.empty() ||
        !has_nondegenerate_triangle(primitive.vertices, binding.indices,
                                    binding.draws)) {
      continue;
    }

    RenderPreviewAsset result{
        .vertices = primitive.vertices,
        .indices = binding.indices,
        .draws = binding.draws,
        .texture = decode_texture_mip(texture, 0),
        .minimum_position =
            {
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
            },
        .maximum_position =
            {
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
            },
        .primitive_packed_index = primitive.packed_index,
        .object_instance = std::nullopt,
    };
    for (const auto index : result.indices) {
      const auto &vertex = result.vertices[index];
      for (std::size_t axis = 0; axis < 3; ++axis) {
        result.minimum_position[axis] =
            std::min(result.minimum_position[axis], vertex.position[axis]);
        result.maximum_position[axis] =
            std::max(result.maximum_position[axis], vertex.position[axis]);
      }
    }
    if (result.minimum_position == result.maximum_position) {
      continue;
    }
    return result;
  }
  return std::nullopt;
}

[[nodiscard]] RenderPreviewAsset bind_diagnostic_source_instance(
    RenderPreviewAsset preview,
    std::span<const data::GmsDirectoryEntry> object_sources) {
  for (std::size_t index = 0; index < object_sources.size(); ++index) {
    const auto &source = object_sources[index];
    if (!source.primitive_reference.has_value() ||
        *source.primitive_reference != preview.primitive_packed_index) {
      continue;
    }
    preview.object_instance = RenderObjectInstance{
        .basis = source.basis,
        .position = source.position,
        .source_type = source.source_type,
        .directory_index = index,
        .local_slot_index = source.local_slot_index,
        .map_instance = std::nullopt,
    };
    return preview;
  }
  throw std::runtime_error(
      "render preview primitive has no GMS object-source instance");
}

} // namespace

std::array<float, 3>
transform_render_position(const RenderObjectInstance &instance,
                          const std::array<float, 3> &local_position) {
  const auto &basis = instance.basis;
  return {
      basis[0] * local_position[0] + basis[1] * local_position[1] +
          basis[2] * local_position[2] + instance.position[0],
      basis[3] * local_position[0] + basis[4] * local_position[1] +
          basis[5] * local_position[2] + instance.position[1],
      basis[6] * local_position[0] + basis[7] * local_position[1] +
          basis[8] * local_position[2] + instance.position[2],
  };
}

void validate_render_preview(const RenderPreviewAsset &preview) {
  if (preview.vertices.empty() || preview.indices.empty() ||
      preview.draws.empty() || preview.texture.width == 0 ||
      preview.texture.height == 0 || !preview.object_instance.has_value()) {
    throw std::invalid_argument("render preview is incomplete");
  }
  const auto width = static_cast<std::size_t>(preview.texture.width);
  const auto height = static_cast<std::size_t>(preview.texture.height);
  if (width > std::numeric_limits<std::size_t>::max() / height ||
      width * height > std::numeric_limits<std::size_t>::max() / 4U ||
      preview.texture.pixels.size() != width * height * 4U) {
    throw std::invalid_argument("render preview has invalid RGBA dimensions");
  }
  const auto finite = [](const auto &values) {
    return std::ranges::all_of(
        values, [](float value) { return std::isfinite(value); });
  };
  const auto &instance = *preview.object_instance;
  if (!finite(instance.basis) || !finite(instance.position) ||
      (instance.map_instance.has_value() &&
       (!finite(instance.map_instance->orientation) ||
        !finite(instance.map_instance->position)))) {
    throw std::invalid_argument("render preview has a non-finite transform");
  }
  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(preview.minimum_position[axis]) ||
        !std::isfinite(preview.maximum_position[axis]) ||
        preview.minimum_position[axis] > preview.maximum_position[axis]) {
      throw std::invalid_argument("render preview has invalid bounds");
    }
  }
  for (const auto &vertex : preview.vertices) {
    if (!finite(vertex.position) || !finite(vertex.texture_coordinates) ||
        !finite(transform_render_position(instance, vertex.position))) {
      throw std::invalid_argument(
          "render preview has a non-finite vertex attribute");
    }
  }
  std::size_t expected_first_index = 0;
  for (const auto &draw : preview.draws) {
    if (draw.first_index != expected_first_index || draw.index_count < 3U ||
        draw.first_index > preview.indices.size() ||
        draw.index_count > preview.indices.size() - draw.first_index) {
      throw std::invalid_argument("render preview has an invalid draw range");
    }
    expected_first_index = draw.first_index + draw.index_count;
  }
  if (expected_first_index != preview.indices.size() ||
      std::ranges::any_of(preview.indices, [&](const auto index) {
        return index >= preview.vertices.size();
      })) {
    throw std::invalid_argument("render preview has an invalid vertex index");
  }
}

RenderPreviewAsset
build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                     std::span<const data::TextureImage> textures) {
  if (auto result = try_build_render_preview(primitives, textures)) {
    return std::move(*result);
  }
  throw std::runtime_error(
      "startup resources contain no supported render-preview primitive");
}

std::vector<SceneGeometryResolution> resolve_scene_geometry_references(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::RenderMapEntry> map_entries) {
  std::unordered_map<std::uint32_t, std::size_t> source_by_slot;
  source_by_slot.reserve(object_sources.size());
  for (std::size_t index = 0; index < object_sources.size(); ++index) {
    if (!source_by_slot.emplace(object_sources[index].local_slot_index, index)
             .second) {
      throw std::runtime_error(
          "scene geometry handle resolves to duplicate GMS local slots");
    }
  }
  std::unordered_map<std::uint32_t, std::size_t> primitive_by_reference;
  primitive_by_reference.reserve(primitives.size());
  for (std::size_t index = 0; index < primitives.size(); ++index) {
    if (!primitive_by_reference.emplace(primitives[index].packed_index, index)
             .second) {
      throw std::runtime_error(
          "scene geometry reference resolves to duplicate PRM indexes");
    }
  }
  std::vector<SceneGeometryResolution> result;
  result.reserve(map_entries.size() * 2U);
  for (std::size_t entry_index = 0; entry_index < map_entries.size();
       ++entry_index) {
    const auto &entry = map_entries[entry_index];
    const std::array references{
        std::pair{SceneGeometryRole::primary,
                  entry.object.primary_geometry_reference},
        std::pair{SceneGeometryRole::secondary,
                  entry.object.secondary_geometry_reference},
    };
    for (const auto &[role, reference] : references) {
      if (role == SceneGeometryRole::secondary && reference == 0) {
        continue;
      }
      SceneGeometryResolution resolution{
          .map_entry_index = entry_index,
          .map_descriptor_offset = entry.descriptor_offset,
          .role = role,
          .geometry_reference = reference,
          .requested_handle_slot_index = 0,
          .status = SceneGeometryStatus::no_local_source,
          .source_directory_index = std::nullopt,
          .source_local_slot_index = std::nullopt,
          .primitive_reference = std::nullopt,
          .primitive_entry_index = std::nullopt,
      };
      const auto handle = data::GmsImage::decode_object_handle(reference);
      resolution.requested_handle_slot_index = handle.slot_index;
      const auto source_lookup = source_by_slot.find(handle.slot_index);
      if (source_lookup == source_by_slot.end()) {
        result.push_back(std::move(resolution));
        continue;
      }
      const auto source_index = source_lookup->second;
      const auto &source = object_sources[source_index];
      resolution.source_directory_index = source_index;
      resolution.source_local_slot_index = source.local_slot_index;
      if (!source.primitive_reference.has_value()) {
        resolution.status = SceneGeometryStatus::source_without_primitive;
        result.push_back(std::move(resolution));
        continue;
      }
      const auto primitive_reference = *source.primitive_reference;
      resolution.primitive_reference = primitive_reference;
      const auto primitive_lookup =
          primitive_by_reference.find(primitive_reference);
      if (primitive_lookup == primitive_by_reference.end()) {
        resolution.status = SceneGeometryStatus::missing_primitive;
        result.push_back(std::move(resolution));
        continue;
      }
      const auto primitive_index = primitive_lookup->second;
      resolution.primitive_entry_index = primitive_index;
      resolution.status = primitives[primitive_index].flagged_reference
                              ? SceneGeometryStatus::unresolved_primitive_alias
                              : SceneGeometryStatus::local_primitive;
      result.push_back(std::move(resolution));
    }
  }
  return result;
}

RenderPreviewAsset build_first_primary_scene_render_preview(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::RenderMapEntry> map_entries) {
  const auto resolutions = resolve_scene_geometry_references(
      primitives, object_sources, map_entries);
  for (const auto &resolution : resolutions) {
    if (resolution.role != SceneGeometryRole::primary ||
        resolution.status != SceneGeometryStatus::local_primitive) {
      continue;
    }
    const auto &entry = map_entries[resolution.map_entry_index];
    const auto &source = object_sources[*resolution.source_directory_index];
    const auto &primitive = primitives[*resolution.primitive_entry_index];
    auto candidate =
        try_build_render_preview(std::span{&primitive, 1}, textures);
    if (!candidate.has_value()) {
      continue;
    }
    auto preview = std::move(*candidate);
    preview.object_instance = RenderObjectInstance{
        .basis = source.basis,
        .position = source.position,
        .source_type = source.source_type,
        .directory_index = *resolution.source_directory_index,
        .local_slot_index = *resolution.source_local_slot_index,
        .map_instance =
            RenderMapInstance{
                .map_entry_index = resolution.map_entry_index,
                .map_descriptor_offset = resolution.map_descriptor_offset,
                .geometry_reference = resolution.geometry_reference,
                .orientation = entry.object.orientation,
                .position = entry.object.position,
            },
    };
    return preview;
  }
  throw NoScenePreviewError{};
}

RenderPreviewAsset
load_startup_render_preview(const std::filesystem::path &install_root) {
  const auto archive =
      data::ZipArchive::open(install_root / "Scenes" / "FF-StartUp.ZIP");
  const auto &primitive_member = unique_member_with_extension(archive, ".prm");
  const auto &texture_member = unique_member_with_extension(archive, ".tex");
  const auto &object_member = unique_member_with_extension(archive, ".gms");
  const auto &map_member = unique_member_with_extension(archive, ".rmc");
  const auto primitive_bytes = archive.read(primitive_member);
  const auto texture_bytes = archive.read(texture_member);
  const auto object_bytes = archive.read(object_member);
  const auto map_bytes = archive.read(map_member);
  const auto primitives = data::PrimitiveCatalog::parse(primitive_bytes);
  const auto textures = data::TextureCatalog::parse(texture_bytes);
  const auto objects =
      data::GmsImage::parse(data::PackedResource::parse(object_bytes));
  const auto map = data::RenderMap::parse(map_bytes);
  try {
    return build_first_primary_scene_render_preview(
        primitives.entries(), textures.images(), objects.directory(),
        map.entries());
  } catch (const NoScenePreviewError &) {
  }
  return bind_diagnostic_source_instance(
      build_render_preview(primitives.entries(), textures.images()),
      objects.directory());
}

} // namespace off::graphics
