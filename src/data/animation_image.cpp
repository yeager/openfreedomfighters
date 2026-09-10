#include "off/data/animation_image.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>

namespace off::data {
namespace {

constexpr std::uint32_t mna_magic = 0x00414e4dU;
constexpr std::uint32_t reference_table_bias = 9U;
constexpr std::uint32_t maximum_reference_tables = 6U;
constexpr std::uint32_t supported_format_value = 10U;
constexpr std::size_t header_size = 20U;
constexpr std::size_t maximum_animation_size = 16U * 1024U * 1024U;
constexpr std::uint32_t maximum_table_entries = 65'536U;
constexpr std::uint32_t minimum_descriptor_count = 3U;
constexpr std::uint32_t maximum_descriptor_count = 24U;

} // namespace

AnimationImage AnimationImage::parse(std::span<const std::byte> bytes) {
  if (bytes.size() < header_size || bytes.size() > maximum_animation_size ||
      (bytes.size() & 3U) != 0U) {
    throw std::runtime_error("invalid animation file size");
  }
  const ByteReader reader(bytes);
  const auto directory_word = reader.u32(12);
  const auto format_value = reader.u32(16);
  if (reader.u32(0) != mna_magic ||
      reader.u32(4) !=
          (0x80000000U | static_cast<std::uint32_t>(bytes.size())) ||
      reader.u32(8) != static_cast<std::uint32_t>(bytes.size()) ||
      directory_word <= reference_table_bias ||
      directory_word > reference_table_bias + maximum_reference_tables ||
      format_value != supported_format_value) {
    throw std::runtime_error("invalid animation file envelope");
  }
  AnimationImage result;
  result.header_ = {.byte_size = bytes.size(),
                    .reference_table_count =
                        directory_word - reference_table_bias,
                    .format_value = format_value};
  std::size_t cursor = header_size;
  result.reference_tables_.reserve(result.header_.reference_table_count);
  for (std::uint32_t table_index = 0;
       table_index < result.header_.reference_table_count; ++table_index) {
    if (cursor > bytes.size() || 24U > bytes.size() - cursor)
      throw std::runtime_error("animation reference table is truncated");
    const auto raw_length = reader.u32(cursor);
    const auto length = static_cast<std::size_t>(raw_length & 0x3fffffffU);
    const auto count = reader.u32(cursor + 12U);
    const auto minimum_length = 28U + static_cast<std::size_t>(count) * 4U;
    if ((raw_length & 0xc0000000U) != 0x80000000U || count == 0U ||
        count > maximum_table_entries || length < 28U ||
        length < minimum_length || length > bytes.size() - cursor ||
        reader.u32(cursor + 4U) != 24U + count * 4U ||
        reader.u32(cursor + 8U) != 1U ||
        reader.u32(cursor + 16U) != 8U + count * 4U)
      throw std::runtime_error("animation reference table is malformed");
    const auto table_end = cursor + length;
    AnimationReferenceTable table;
    table.reference_words.reserve(count);
    std::unordered_set<std::uint32_t> unique_references;
    unique_references.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
      const auto value = reader.u32(cursor + 20U + index * 4U);
      if ((value & 3U) != 0U || value < table_end || value >= bytes.size() ||
          !unique_references.insert(value).second)
        throw std::runtime_error("animation reference word is invalid");
      table.reference_words.push_back(value);
    }
    const auto name_start = cursor + 20U + static_cast<std::size_t>(count) * 4U;
    const auto marker_offset = table_end - 4U;
    auto terminator = name_start;
    while (terminator < marker_offset && bytes[terminator] != std::byte{0})
      ++terminator;
    if (terminator == marker_offset || terminator == name_start)
      throw std::runtime_error("animation reference name is invalid");
    const auto name_bytes = terminator - name_start + 1U;
    const auto aligned_name_bytes = (name_bytes + 3U) & ~std::size_t{3U};
    if (length !=
        24U + static_cast<std::size_t>(count) * 4U + aligned_name_bytes)
      throw std::runtime_error("animation reference table length is invalid");
    table.name.assign(reinterpret_cast<const char *>(bytes.data() + name_start),
                      terminator - name_start);
    if (table.name.size() < 4U ||
        table.name.substr(table.name.size() - 4U) != ".anm" ||
        !std::ranges::all_of(table.name,
                             [](unsigned char value) {
                               return value >= 0x20U && value <= 0x7eU;
                             }) ||
        !std::ranges::all_of(
            bytes.subspan(terminator + 1U, marker_offset - terminator - 1U),
            [](std::byte value) { return value == std::byte{0}; }))
      throw std::runtime_error("animation reference name is invalid");
    const auto expected_marker =
        table_index + 1U == result.header_.reference_table_count ? 14U : 10U;
    if (reader.u32(marker_offset) != expected_marker)
      throw std::runtime_error("animation reference table marker is invalid");
    result.reference_tables_.push_back(std::move(table));
    cursor = table_end;
  }
  if (cursor > bytes.size() || 8U > bytes.size() - cursor)
    throw std::runtime_error("animation descriptor block is truncated");
  const auto descriptor_length = static_cast<std::size_t>(reader.u32(cursor));
  if (descriptor_length < 8U || descriptor_length > bytes.size() - cursor ||
      (descriptor_length - 8U) % 12U != 0U || reader.u32(cursor + 4U) != 0U)
    throw std::runtime_error("animation descriptor block is malformed");
  const auto descriptor_count = (descriptor_length - 8U) / 12U;
  if (descriptor_count < minimum_descriptor_count ||
      descriptor_count > maximum_descriptor_count)
    throw std::runtime_error("animation descriptor count is unsupported");
  result.descriptors_.reserve(descriptor_count);
  for (std::size_t index = 0; index < descriptor_count; ++index) {
    const auto offset = cursor + 8U + index * 12U;
    const auto tag = reader.u32(offset + 8U);
    if ((tag == 7U) != (index + 1U == descriptor_count))
      throw std::runtime_error("animation descriptor terminator is invalid");
    result.descriptors_.push_back({.opaque_word_0 = reader.u32(offset),
                                   .opaque_word_1 = reader.u32(offset + 4U),
                                   .tag = tag});
  }
  return result;
}

} // namespace off::data
