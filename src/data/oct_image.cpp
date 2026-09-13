#include "off/data/oct_image.hpp"

#include "off/data/byte_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::size_t minimum_oct_image_bytes = 100U;
constexpr std::size_t maximum_oct_image_bytes = 4U * 1024U * 1024U;
constexpr std::size_t structural_header_bytes = 52U;

} // namespace

OctImage OctImage::parse(std::span<const std::byte> bytes) {
  if (bytes.size() < minimum_oct_image_bytes ||
      bytes.size() > maximum_oct_image_bytes || (bytes.size() & 7U) != 4U)
    throw std::runtime_error("OCT image size is unsupported");

  const ByteReader reader(bytes);
  const auto repeated_flag = reader.u32(0U);
  if (repeated_flag > 1U || reader.u32(4U) != repeated_flag ||
      reader.u32(8U) != repeated_flag || reader.u32(12U) != repeated_flag ||
      reader.u32(36U) != 0U || reader.u32(40U) != 0U ||
      reader.u32(44U) != 0U)
    throw std::runtime_error("OCT image header is invalid");

  const auto upper = static_cast<std::size_t>(reader.u32(16U));
  const auto lower = static_cast<std::size_t>(reader.u32(48U));
  if ((lower == 0U) != (upper == 0U))
    throw std::runtime_error("OCT image structural bounds disagree");

  OctImage result;
  result.envelope_.byte_size = bytes.size();
  result.envelope_.repeated_header_flag = repeated_flag != 0U;
  if (lower == 0U)
    return result;
  if (lower < structural_header_bytes || (lower & 3U) != 0U ||
      (upper & 3U) != 0U || lower >= upper || upper >= bytes.size())
    throw std::runtime_error("OCT image structural bounds are invalid");
  result.envelope_.structural_bounds = {.lower = lower, .upper = upper};
  return result;
}

} // namespace off::data
