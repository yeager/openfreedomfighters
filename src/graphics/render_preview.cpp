#include "off/graphics/render_preview.hpp"

#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
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
        .map_entry_index = 0,
        .map_descriptor_offset = 0,
        .geometry_reference = 0,
        .map_orientation = {},
        .map_position = {},
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

RenderPreviewAsset
build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                     std::span<const data::TextureImage> textures) {
  if (auto result = try_build_render_preview(primitives, textures)) {
    return std::move(*result);
  }
  throw std::runtime_error(
      "startup resources contain no supported render-preview primitive");
}

RenderPreviewAsset build_first_scene_render_preview(
    std::span<const data::PrimitiveEntry> primitives,
    std::span<const data::TextureImage> textures,
    std::span<const data::GmsDirectoryEntry> object_sources,
    std::span<const data::RenderMapEntry> map_entries) {
  for (std::size_t entry_index = 0; entry_index < map_entries.size();
       ++entry_index) {
    const auto &entry = map_entries[entry_index];
    const auto handle = data::GmsImage::decode_object_handle(
        entry.object.primary_geometry_reference);
    const auto source_match =
        std::ranges::find_if(object_sources, [&](const auto &source) {
          return source.local_slot_index == handle.slot_index;
        });
    if (source_match != object_sources.end() &&
        std::ranges::find_if(std::next(source_match), object_sources.end(),
                             [&](const auto &source) {
                               return source.local_slot_index ==
                                      handle.slot_index;
                             }) != object_sources.end()) {
      throw std::runtime_error(
          "scene geometry handle resolves to duplicate GMS local slots");
    }
    if (source_match == object_sources.end() ||
        !source_match->primitive_reference.has_value()) {
      continue;
    }
    const auto primitive_reference = *source_match->primitive_reference;
    const auto first_primitive =
        std::ranges::find_if(primitives, [&](const auto &primitive) {
          return primitive.packed_index == primitive_reference;
        });
    if (first_primitive == primitives.end()) {
      continue;
    }
    if (std::ranges::find_if(std::next(first_primitive), primitives.end(),
                             [&](const auto &primitive) {
                               return primitive.packed_index ==
                                      primitive_reference;
                             }) != primitives.end()) {
      throw std::runtime_error(
          "scene geometry reference resolves to duplicate PRM indexes");
    }
    auto candidate =
        try_build_render_preview(std::span{&*first_primitive, 1}, textures);
    if (!candidate.has_value()) {
      continue;
    }
    auto preview = std::move(*candidate);
    preview.object_instance = RenderObjectInstance{
        .basis = source_match->basis,
        .position = source_match->position,
        .source_type = source_match->source_type,
        .directory_index = static_cast<std::size_t>(
            std::distance(object_sources.begin(), source_match)),
        .local_slot_index = source_match->local_slot_index,
        .map_entry_index = entry_index,
        .map_descriptor_offset = entry.descriptor_offset,
        .geometry_reference = entry.object.primary_geometry_reference,
        .map_orientation = entry.object.orientation,
        .map_position = entry.object.position,
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
    return build_first_scene_render_preview(primitives.entries(),
                                            textures.images(),
                                            objects.directory(), map.entries());
  } catch (const NoScenePreviewError &) {
  }
  return bind_diagnostic_source_instance(
      build_render_preview(primitives.entries(), textures.images()),
      objects.directory());
}

} // namespace off::graphics
