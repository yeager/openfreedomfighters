#include "off/graphics/startup_graphics_asset.hpp"

#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace off::graphics {
namespace {

[[nodiscard]] std::string lowercase(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

[[nodiscard]] const data::ZipEntry &
unique_member(const data::ZipArchive &archive, std::string_view extension) {
  const data::ZipEntry *found = nullptr;
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot == std::string::npos ||
        lowercase(entry.name.substr(dot)) != extension)
      continue;
    if (found != nullptr)
      throw std::runtime_error("startup archive has duplicate " +
                               std::string(extension) + " members");
    found = &entry;
  }
  if (found == nullptr)
    throw std::runtime_error("startup archive has no " +
                             std::string(extension) + " member");
  return *found;
}

[[nodiscard]] std::vector<std::size_t> image_indexes(
    const data::StartupGraphicsComposition &composition) {
  std::vector<std::size_t> indexes;
  indexes.reserve(startup_graphics_image_count);
  for (const auto &row : composition.rows()) {
    for (const auto &picture : row.pictures) {
      for (const auto &group : picture.draw_plan.groups()) {
        if (std::ranges::find(indexes, group.texture.image_index) ==
            indexes.end())
          indexes.push_back(group.texture.image_index);
      }
    }
  }
  if (indexes.size() != startup_graphics_image_count)
    throw std::runtime_error(
        "startup graphics composition does not reference exactly six images");
  std::ranges::sort(indexes);
  return indexes;
}

} // namespace

StartupGraphicsAsset build_startup_graphics_asset(
    data::StartupGraphicsComposition composition,
    const data::TextureCatalog &textures, std::size_t decoded_byte_budget) {
  const auto indexes = image_indexes(composition);
  std::size_t total = 0;
  for (const auto index : indexes) {
    if (index >= textures.images().size())
      throw std::runtime_error("startup graphics image index is out of range");
    const auto &image = textures.images()[index];
    const auto bytes = static_cast<std::uint64_t>(image.width) * image.height * 4U;
    if (bytes == 0 || bytes > std::numeric_limits<std::size_t>::max() ||
        bytes > decoded_byte_budget - std::min(total, decoded_byte_budget))
      throw std::runtime_error(
          "startup graphics decoded images exceed the upload budget");
    total += static_cast<std::size_t>(bytes);
  }

  StartupGraphicsAsset result;
  result.composition_ = std::move(composition);
  for (std::size_t output = 0; output < indexes.size(); ++output) {
    const auto index = indexes[output];
    auto decoded = decode_texture_mip(textures.images()[index], 0);
    const auto expected = static_cast<std::uint64_t>(decoded.width) *
                          decoded.height * 4U;
    if (decoded.width != textures.images()[index].width ||
        decoded.height != textures.images()[index].height ||
        expected != decoded.pixels.size())
      throw std::runtime_error(
          "startup graphics image decoded to an invalid extent");
    result.images_[output] = {index, textures.images()[index].id,
                              std::move(decoded)};
  }
  return result;
}

StartupGraphicsAsset
load_startup_graphics_asset(const std::filesystem::path &startup_archive) {
  const auto archive = data::ZipArchive::open(startup_archive);
  const auto &gms_member = unique_member(archive, ".gms");
  const auto &buf_member = unique_member(archive, ".buf");
  const auto &prm_member = unique_member(archive, ".prm");
  const auto &tex_member = unique_member(archive, ".tex");

  auto gms = data::GmsImage::parse(
      data::PackedResource::parse(archive.read(gms_member)));
  const auto buf = archive.read(buf_member);
  gms.validate_buf(buf);
  const auto primitive_allocation = archive.read(prm_member);
  const auto textures = data::TextureCatalog::parse(archive.read(tex_member));
  auto composition = data::StartupGraphicsComposition::build(
      gms, primitive_allocation, textures);
  return build_startup_graphics_asset(std::move(composition), textures);
}

} // namespace off::graphics
