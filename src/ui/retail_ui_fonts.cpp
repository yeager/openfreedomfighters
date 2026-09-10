#include "off/ui/retail_ui_fonts.hpp"

#include "off/data/packed_resource.hpp"
#include "off/data/zgf_bundle.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

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

[[nodiscard]] bool range(std::span<const std::byte> bytes, std::size_t offset, std::size_t size) noexcept {
  return offset <= bytes.size() && size <= bytes.size() - offset;
}
[[nodiscard]] std::optional<std::span<const std::byte>> cmap_table(std::span<const std::byte> bytes) noexcept {
  if (!is_bounded_sfnt(bytes)) return std::nullopt;
  const auto count = be16(bytes, 4);
  for (std::size_t index = 0; index < count; ++index) {
    const auto record = 12U + index * 16U;
    if (be32(bytes, record) != 0x636d6170U) continue;
    const auto offset = static_cast<std::size_t>(be32(bytes, record + 8));
    const auto length = static_cast<std::size_t>(be32(bytes, record + 12));
    if (!range(bytes, offset, length) || length < 4U) return std::nullopt;
    return bytes.subspan(offset, length);
  }
  return std::nullopt;
}
[[nodiscard]] bool cmap4_has(std::span<const std::byte> table, std::uint32_t scalar) noexcept {
  if (scalar > 0xffffU || !range(table, 0, 16U) || be16(table, 0) != 4U) return false;
  const auto length = static_cast<std::size_t>(be16(table, 2));
  const auto segments = static_cast<std::size_t>(be16(table, 6)) / 2U;
  if (!range(table, 0, length) || segments == 0U || !range(table, 14U, segments * 8U)) return false;
  const auto ends = 14U;
  const auto starts = ends + segments * 2U + 2U;
  const auto deltas = starts + segments * 2U;
  const auto ranges = deltas + segments * 2U;
  for (std::size_t index = 0; index < segments; ++index) {
    const auto start = be16(table, starts + 2U * index);
    const auto end = be16(table, ends + 2U * index);
    if (scalar < start || scalar > end) continue;
    const auto range_offset = be16(table, ranges + 2U * index);
    if (range_offset == 0U) return true;
    const auto glyph = ranges + 2U * index + range_offset + 2U * (scalar - start);
    return range(table, glyph, 2U) && be16(table, glyph) != 0U;
  }
  return false;
}
[[nodiscard]] bool cmap12_has(std::span<const std::byte> table, std::uint32_t scalar) noexcept {
  if (!range(table, 0, 16U) || be16(table, 0) != 12U) return false;
  const auto length = static_cast<std::size_t>(be32(table, 4));
  const auto groups = static_cast<std::size_t>(be32(table, 12));
  if (!range(table, 0, length) || groups > (table.size() - 16U) / 12U) return false;
  for (std::size_t index = 0; index < groups; ++index) {
    const auto group = 16U + index * 12U;
    const auto start = be32(table, group), end = be32(table, group + 4U);
    if (start > end) return false;
    if (scalar >= start && scalar <= end) return true;
  }
  return false;
}
[[nodiscard]] bool font_has(std::span<const std::byte> font, std::uint32_t scalar) noexcept {
  const auto cmap = cmap_table(font);
  if (!cmap || !range(*cmap, 0, 4U)) return false;
  const auto records = be16(*cmap, 2);
  if (!range(*cmap, 4U, static_cast<std::size_t>(records) * 8U)) return false;
  for (std::size_t index = 0; index < records; ++index) {
    const auto offset = static_cast<std::size_t>(be32(*cmap, 8U + index * 8U));
    if (!range(*cmap, offset, 2U)) continue;
    const auto subtable = cmap->subspan(offset);
    if (cmap12_has(subtable, scalar) || cmap4_has(subtable, scalar)) return true;
  }
  return false;
}
[[nodiscard]] bool next_utf8(std::string_view text, std::size_t &offset, std::uint32_t &scalar) noexcept {
  if (offset == text.size()) return false;
  const auto lead = static_cast<unsigned char>(text[offset++]);
  if (lead < 0x80U) { scalar = lead; return true; }
  const auto count = lead >= 0xc2U && lead <= 0xdfU ? 1U : lead >= 0xe0U && lead <= 0xefU ? 2U : lead >= 0xf0U && lead <= 0xf4U ? 3U : 4U;
  if (count == 4U || offset + count > text.size()) return false;
  scalar = lead & (count == 1U ? 0x1fU : count == 2U ? 0x0fU : 0x07U);
  for (unsigned index = 0; index < count; ++index) { const auto value = static_cast<unsigned char>(text[offset++]); if ((value & 0xc0U) != 0x80U) return false; scalar = (scalar << 6U) | (value & 0x3fU); }
  const auto minimum = count == 1U ? 0x80U : count == 2U ? 0x800U : 0x10000U;
  return scalar >= minimum && scalar <= 0x10ffffU && !(scalar >= 0xd800U && scalar <= 0xdfffU);
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

std::optional<std::size_t> select_font_for_utf8(const RetailUiFontSet &fonts, std::string_view text) noexcept {
  std::vector<std::uint32_t> scalars;
  for (std::size_t offset = 0; offset < text.size();) { std::uint32_t scalar{}; if (!next_utf8(text, offset, scalar)) return std::nullopt; scalars.push_back(scalar); }
  for (std::size_t index = 0; index < fonts.fonts.size(); ++index)
    if (std::ranges::all_of(scalars, [&](const auto scalar) { return font_has(fonts.fonts[index].sfnt, scalar); })) return index;
  return std::nullopt;
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
