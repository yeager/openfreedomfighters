#pragma once

#include "off/data/deferred_compact_block_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace off::graphics {

// The reviewed intro has a named/global section whose caller supplies both
// GMS-relative boundaries. The only admitted layout here is a leading
// NUL-terminated label followed by one bounded tagged block. The block header
// is part of the typed reader's input. This is deliberately not a tagged-value
// parser or a general GMS section schema.
struct IntroNamedGlobalSectionEnvelope {
  std::string_view label;
  std::span<const std::byte> tagged_block;
};

// An owned, mutable view handed through the relocation and typed-reader
// boundaries. Relocation may advance cursor while traversing the block; the
// caller that prepares this view restores it to zero before the typed reader
// observes it. No tag semantics are implemented by this type.
struct IntroNamedGlobalPreparedReader {
  std::vector<std::byte> owned_block;
  std::size_t cursor{};

  [[nodiscard]] std::span<std::byte> mutable_block() { return owned_block; }
  [[nodiscard]] std::span<const std::byte> complete_block() const { return owned_block; }
  void reset_to_base() { cursor=0; }
};

// A payload-free structural profile for the recovered tagged block. The label
// remains deliberately outside the result: it is source content, while this
// type only reports framing that can guide a future concrete typed reader.
struct IntroNamedGlobalSectionProfile final {
  std::size_t body_bytes{};
  data::DeferredCompactBlockProfile tagged_values;

  [[nodiscard]] bool operator==(const IntroNamedGlobalSectionProfile&) const = default;
};

inline IntroNamedGlobalSectionEnvelope parse_intro_named_global_section_envelope(
    std::span<const std::byte> section) {
  std::size_t label_end=0;
  while(label_end<section.size() && section[label_end]!=std::byte{0}) ++label_end;
  if(label_end==section.size())
    throw std::runtime_error("Named/global section label is not NUL-terminated within its bounds");

  const auto after_label=label_end+1U;
  constexpr std::size_t tagged_block_header_size=4U;
  if(section.size()-after_label<tagged_block_header_size)
    throw std::runtime_error("Named/global section lacks a complete tagged-block header");
  std::uint32_t header{};
  for(std::size_t byte=0;byte<tagged_block_header_size;++byte)
    header|=static_cast<std::uint32_t>(std::to_integer<unsigned char>(
        section[after_label+byte])) << (byte*8U);
  const auto block_size=static_cast<std::size_t>(header&0x00ffffffU);
  if(block_size<tagged_block_header_size || block_size>section.size()-after_label)
    throw std::runtime_error("Named/global tagged block exceeds its section");

  const auto label_data=reinterpret_cast<const char*>(section.data());
  return {
      .label=std::string_view(label_data,label_end),
      .tagged_block=section.subspan(after_label,block_size)};
}

inline IntroNamedGlobalSectionEnvelope parse_intro_named_global_section_envelope(
    std::span<const std::byte> decoded_gms_image, std::uint32_t section_offset,
    std::uint32_t next_section_offset) {
  if(section_offset==0 || next_section_offset==0)
    throw std::runtime_error("Named/global section requires explicit nonzero bounds");

  const auto begin=static_cast<std::size_t>(section_offset);
  const auto end=static_cast<std::size_t>(next_section_offset);
  if(begin>=end || end>decoded_gms_image.size())
    throw std::runtime_error("Named/global section bounds are outside the decoded GMS image");
  return parse_intro_named_global_section_envelope(
      decoded_gms_image.subspan(begin,end-begin));
}

inline IntroNamedGlobalSectionProfile profile_intro_named_global_section(
    const IntroNamedGlobalSectionEnvelope& envelope) {
  constexpr std::size_t tagged_block_header_size=4U;
  if(envelope.tagged_block.size()<=tagged_block_header_size)
    throw std::runtime_error("Named/global tagged block has no body to profile");
  const auto body=envelope.tagged_block.subspan(tagged_block_header_size);
  return {
      .body_bytes=body.size(),
      .tagged_values=data::DeferredCompactBlockProfiler::profile(body)};
}

}  // namespace off::graphics
