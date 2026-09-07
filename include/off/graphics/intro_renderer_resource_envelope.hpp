#pragma once

#include "off/graphics/intro_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace off::graphics {

// The supported ordinary loader treats the nonzero scene-header word at 0x18
// as a GMS-relative renderer section. Its first little-endian word frames an
// otherwise opaque payload. This validates and copies that envelope only; the
// parser remains the required concrete renderer-container boundary.
inline IntroRendererResourceContainer parse_intro_renderer_resource_envelope(
    std::span<const std::byte> decoded_gms_image, std::uint32_t section_offset,
    const std::function<IntroRendererResourceContainer(std::span<const std::byte>)>& parser) {
  if(section_offset==0) throw std::runtime_error("Renderer-resource envelope requires a present nonzero section offset");
  const auto offset=static_cast<std::size_t>(section_offset);
  if(offset>decoded_gms_image.size() || decoded_gms_image.size()-offset<sizeof(std::uint32_t))
    throw std::runtime_error("Renderer-resource envelope count is outside the decoded GMS image");

  std::uint32_t encoded_count{};
  for(std::size_t byte=0;byte<sizeof(encoded_count);++byte)
    encoded_count|=static_cast<std::uint32_t>(std::to_integer<unsigned char>(decoded_gms_image[offset+byte]))
        << (byte*8U);
  const auto payload_offset=offset+sizeof(encoded_count);
  const auto payload_size=static_cast<std::size_t>(encoded_count);
  if(payload_size>decoded_gms_image.size()-payload_offset)
    throw std::runtime_error("Renderer-resource envelope payload exceeds the decoded GMS image");
  if(!parser) throw std::runtime_error("Renderer-resource envelope requires a concrete parser");

  // Do not borrow source bytes across the parser boundary: the original loader
  // constructs an independent representation before handing it to the live
  // container. No bytes outside this exact payload range are observable here.
  const std::vector<std::byte> copied_payload(
      decoded_gms_image.begin()+static_cast<std::ptrdiff_t>(payload_offset),
      decoded_gms_image.begin()+static_cast<std::ptrdiff_t>(payload_offset+payload_size));
  return parser(copied_payload);
}

}  // namespace off::graphics
