#include "off/ui/retail_ui_textures.hpp"

#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace off::ui {
namespace {

constexpr std::size_t role_count = 16;

[[nodiscard]] const data::ZipEntry &startup_texture_member(
    const data::ZipArchive &archive) {
  constexpr std::array<std::string_view, 11> required_members{
      "SCENES/FF-StartUp.ZGF", "SCENES/FF-StartUp.SUP",
      "SCENES/FF-StartUp.BUF", "SCENES/FF-StartUp.GMS",
      "SCENES/FF-StartUp.TEX", "SCENES/FF-StartUp.SND",
      "SCENES/FF-StartUp.LOC", "SCENES/FF-StartUp.OCT",
      "SCENES/FF-StartUp.SGP", "SCENES/FF-StartUp.RMC",
      "SCENES/FF-StartUp.RMI"};
  if (archive.entries().size() != required_members.size())
    throw std::runtime_error("retail UI texture archive is not the canonical startup family");

  const data::ZipEntry *texture = nullptr;
  for (const auto expected : required_members) {
    const auto count = static_cast<std::size_t>(std::count_if(
        archive.entries().begin(), archive.entries().end(),
        [expected](const data::ZipEntry &entry) { return entry.name == expected; }));
    if (count != 1U)
      throw std::runtime_error("retail UI texture archive is not the canonical startup family");
    if (expected.ends_with(".TEX"))
      texture = archive.find(expected);
  }
  if (texture == nullptr)
    throw std::runtime_error("retail UI texture archive is not the canonical startup family");
  return *texture;
}

[[nodiscard]] bool is_size(const data::TextureImage &image, std::uint32_t width,
                           std::uint32_t height) noexcept {
  return image.width == width && image.height == height &&
         !image.mips.empty() && image.mips.front().width == width &&
         image.mips.front().height == height;
}

[[nodiscard]] bool valid_dimensions(RetailUiTextureRole role,
                                    const data::TextureImage &image) noexcept {
  switch (role) {
  case RetailUiTextureRole::scanlines_top:
  case RetailUiTextureRole::scanlines_bottom:
    return is_size(image, 128, 128);
  case RetailUiTextureRole::black_fill_top:
  case RetailUiTextureRole::black_fill_bottom:
  case RetailUiTextureRole::arrow_left:
  case RetailUiTextureRole::arrow_right:
  case RetailUiTextureRole::arrow_up:
  case RetailUiTextureRole::arrow_down:
    return is_size(image, 16, 16);
  default:
    return is_size(image, 2, 2);
  }
}

} // namespace

const RetailUiTexture *
RetailUiTextureSet::find(RetailUiTextureRole role) const noexcept {
  const auto found = std::ranges::find(textures_, role, &RetailUiTexture::role);
  return found == textures_.end() ? nullptr : &*found;
}

RetailUiTextureSet
resolve_retail_ui_textures(std::span<const data::TextureImage> images,
                           std::span<const RetailUiTextureBinding> bindings) {
  if (bindings.size() != role_count)
    throw std::runtime_error("retail UI texture role mapping is incomplete");
  std::array<std::optional<std::size_t>, role_count> resolved;
  for (const auto &binding : bindings) {
    const auto role_index = static_cast<std::size_t>(binding.role);
    if (role_index >= resolved.size() || binding.image_index >= images.size())
      throw std::runtime_error("retail UI texture binding is out of range");
    if (resolved[role_index])
      throw std::runtime_error("retail UI texture role is ambiguous");
    if (!valid_dimensions(binding.role, images[binding.image_index]))
      throw std::runtime_error("retail UI texture has invalid dimensions");
    for (std::size_t other = 0; other < resolved.size(); ++other) {
      if (!resolved[other] || *resolved[other] != binding.image_index)
        continue;
      throw std::runtime_error("retail UI texture binding is reused");
    }
    resolved[role_index] = binding.image_index;
  }

  RetailUiTextureSet out;
  out.textures_.reserve(role_count);
  for (std::size_t index = 0; index < resolved.size(); ++index) {
    if (!resolved[index])
      throw std::runtime_error("retail UI texture role is missing");
    out.textures_.push_back(
        {static_cast<RetailUiTextureRole>(index),
         graphics::decode_texture_mip(images[*resolved[index]], 0)});
  }
  return out;
}

RetailUiTextureSet
load_retail_ui_textures(const std::filesystem::path &startup_archive,
                        std::span<const RetailUiTextureBinding> bindings) {
  const auto archive = data::ZipArchive::open(startup_archive);
  const auto bytes = archive.read(startup_texture_member(archive));
  const auto catalog = data::TextureCatalog::parse(bytes);
  return resolve_retail_ui_textures(catalog.images(), bindings);
}

} // namespace off::ui
