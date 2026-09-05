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
    if (texture.mips.empty()) {
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
    for (const auto &vertex : result.vertices) {
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
