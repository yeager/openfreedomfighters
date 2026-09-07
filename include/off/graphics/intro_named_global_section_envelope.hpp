#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>

namespace off::graphics {

// The reviewed intro has a named/global section whose caller supplies both
// GMS-relative boundaries.  The only admitted layout here is its leading
// NUL-terminated label followed by a four-byte reader prelude.  The remaining
// bounded bytes belong to a still-required typed reader: this is deliberately
// not a tagged-value parser or a general GMS section schema.
struct IntroNamedGlobalSectionEnvelope {
  std::string_view relocated_label;
  std::span<const std::byte> typed_reader_span;
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
  constexpr std::size_t reader_prelude_size=4U;
  if(reader_prelude_size>end-after_label)
    throw std::runtime_error("Named/global section lacks its fixed reader prelude");

  const auto label_data=reinterpret_cast<const char*>(decoded_gms_image.data()+
                                                       static_cast<std::ptrdiff_t>(begin));
  return {
      .relocated_label=std::string_view(label_data,label_end-begin),
      .typed_reader_span=decoded_gms_image.subspan(after_label+reader_prelude_size,
                                                   end-(after_label+reader_prelude_size))};
}

}  // namespace off::graphics
