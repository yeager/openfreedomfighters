#include "off/data/animation_image.hpp"

#include "off/data/byte_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::uint32_t mna_magic = 0x00414e4dU;
constexpr std::uint32_t supported_major_version = 12U;
constexpr std::uint32_t supported_minor_version = 10U;
constexpr std::size_t header_size = 20U;
constexpr std::size_t maximum_animation_size = 16U * 1024U * 1024U;

} // namespace

AnimationImage AnimationImage::parse(std::span<const std::byte> bytes) {
  if (bytes.size() < header_size || bytes.size() > maximum_animation_size ||
      (bytes.size() & 3U) != 0U) {
    throw std::runtime_error("invalid animation file size");
  }
  const ByteReader reader(bytes);
  if (reader.u32(0) != mna_magic ||
      reader.u32(8) != static_cast<std::uint32_t>(bytes.size()) ||
      reader.u32(12) != supported_major_version ||
      reader.u32(16) != supported_minor_version) {
    throw std::runtime_error("invalid animation file envelope");
  }
  AnimationImage result;
  result.header_ = {.byte_size = bytes.size(),
                    .major_version = supported_major_version,
                    .minor_version = supported_minor_version};
  return result;
}

} // namespace off::data
