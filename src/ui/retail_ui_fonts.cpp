#include "off/ui/retail_ui_fonts.hpp"

#include "off/data/packed_resource.hpp"
#include "off/data/zgf_bundle.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace off::ui {
namespace {

constexpr std::size_t maximum_fonts = 32;
constexpr std::size_t maximum_font_bytes = 32U * 1024U * 1024U;

[[nodiscard]] std::uint16_t be16(std::span<const std::byte> bytes,
                                 std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(
      (std::to_integer<std::uint16_t>(bytes[offset]) << 8U) |
      std::to_integer<std::uint16_t>(bytes[offset + 1]));
}

[[nodiscard]] std::uint32_t be32(std::span<const std::byte> bytes,
                                 std::size_t offset) noexcept {
  std::uint32_t value{};
  for (std::size_t index = 0; index < 4; ++index)
    value =
        (value << 8U) | std::to_integer<std::uint8_t>(bytes[offset + index]);
  return value;
}

[[nodiscard]] std::string lowercase(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

[[nodiscard]] bool has_extension(std::string_view name,
                                 std::string_view extension) {
  const auto dot = name.find_last_of('.');
  return dot != std::string_view::npos &&
         lowercase(std::string{name.substr(dot)}) == extension;
}

} // namespace

bool is_bounded_sfnt(std::span<const std::byte> bytes) noexcept {
  if (bytes.size() < 12 || bytes.size() > maximum_font_bytes)
    return false;
  const auto signature = be32(bytes, 0);
  if (signature != 0x00010000U && signature != 0x4f54544fU)
    return false;
  const auto table_count = be16(bytes, 4);
  if (table_count == 0 || table_count > 4096 ||
      static_cast<std::size_t>(table_count) > (bytes.size() - 12) / 16)
    return false;
  for (std::size_t index = 0; index < table_count; ++index) {
    const auto record = 12 + index * 16;
    const auto offset = static_cast<std::size_t>(be32(bytes, record + 8));
    const auto length = static_cast<std::size_t>(be32(bytes, record + 12));
    if (offset > bytes.size() || length > bytes.size() - offset)
      return false;
  }
  return true;
}

RetailUiFontSet
load_retail_ui_fonts(const std::filesystem::path &startup_archive) {
  const auto archive = data::ZipArchive::open(startup_archive);
  const data::ZipEntry *zgf_entry = nullptr;
  for (const auto &entry : archive.entries()) {
    if (!has_extension(entry.name, ".zgf"))
      continue;
    if (zgf_entry != nullptr)
      throw std::runtime_error("startup archive has multiple ZGF members");
    zgf_entry = &entry;
  }
  if (zgf_entry == nullptr)
    throw std::runtime_error("startup archive has no ZGF member");

  const auto bundle = data::ZgfBundle::parse(
      data::PackedResource::parse(archive.read(*zgf_entry)));
  RetailUiFontSet result;
  for (std::size_t index = 0; index < bundle.entries().size(); ++index) {
    const auto &entry = bundle.entries()[index];
    if (!has_extension(entry.name, ".ttf") &&
        !has_extension(entry.name, ".otf"))
      continue;
    if (result.fonts.size() >= maximum_fonts)
      throw std::runtime_error("startup archive has too many embedded fonts");
    const auto payload = bundle.entry_payload(index);
    if (!is_bounded_sfnt(payload))
      throw std::runtime_error("startup archive has an invalid embedded font");
    result.fonts.push_back(
        {std::vector<std::byte>(payload.begin(), payload.end())});
  }
  if (result.fonts.empty())
    throw std::runtime_error("startup archive has no valid embedded fonts");
  return result;
}

} // namespace off::ui
