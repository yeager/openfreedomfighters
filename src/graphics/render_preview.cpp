#include "off/graphics/render_preview.hpp"

#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>

namespace off::graphics {
namespace {

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

} // namespace

RenderPreviewAsset
build_render_preview(std::span<const data::PrimitiveEntry> primitives,
                     std::span<const data::TextureImage> textures) {
  const auto bindings = RenderAssetBindings::build(primitives, textures);
  for (const auto &binding : bindings.primitives()) {
    if (binding.topology != PrimitiveTopology::triangle_strip ||
        !binding.texture_image_index.has_value()) {
      continue;
    }
    const auto &primitive = primitives[binding.primitive_entry_index];
    const auto &texture = textures[*binding.texture_image_index];
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
  throw std::runtime_error(
      "startup resources contain no supported render-preview primitive");
}

RenderPreviewAsset
load_startup_render_preview(const std::filesystem::path &install_root) {
  const auto archive =
      data::ZipArchive::open(install_root / "Scenes" / "FF-StartUp.ZIP");
  const auto &primitive_member = unique_member_with_extension(archive, ".prm");
  const auto &texture_member = unique_member_with_extension(archive, ".tex");
  const auto primitive_bytes = archive.read(primitive_member);
  const auto texture_bytes = archive.read(texture_member);
  const auto primitives = data::PrimitiveCatalog::parse(primitive_bytes);
  const auto textures = data::TextureCatalog::parse(texture_bytes);
  return build_render_preview(primitives.entries(), textures.images());
}

} // namespace off::graphics
