#pragma once

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

inline IntroNamedGlobalSectionEnvelope parse_intro_named_global_section_envelope(
    std::span<const std::byte> decoded_gms_image, std::uint32_t section_offset,
    std::uint32_t next_section_offset) {
  if(section_offset==0 || next_section_offset==0)
    throw std::runtime_error("Named/global section requires explicit nonzero bounds");

  const auto begin=static_cast<std::size_t>(section_offset);
  const auto end=static_cast<std::size_t>(next_section_offset);
  if(begin>=end || end>decoded_gms_image.size())
    throw std::runtime_error("Named/global section bounds are outside the decoded GMS image");

  std::size_t label_end=begin;
  while(label_end<end && decoded_gms_image[label_end]!=std::byte{0}) ++label_end;
  if(label_end==end)
    throw std::runtime_error("Named/global section label is not NUL-terminated within its bounds");

  const auto after_label=label_end+1U;
  constexpr std::size_t tagged_block_header_size=4U;
  if(end-after_label<tagged_block_header_size)
    throw std::runtime_error("Named/global section lacks a complete tagged-block header");

  const auto label_data=reinterpret_cast<const char*>(decoded_gms_image.data()+
                                                       static_cast<std::ptrdiff_t>(begin));
  return {
      .label=std::string_view(label_data,label_end-begin),
      .tagged_block=decoded_gms_image.subspan(after_label,end-after_label)};
}

}  // namespace off::graphics
