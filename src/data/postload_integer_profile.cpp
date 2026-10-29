#include "off/data/postload_integer_profile.hpp"

#include <algorithm>
#include <string_view>

namespace off::data {
std::optional<PostloadIntegerProfile> parse_postload_integer_profile(
    std::span<const std::byte> frame) {
  const auto byte=[&](std::size_t i) {return std::to_integer<std::uint8_t>(frame[i]);};
  if(frame.size()<4U) return std::nullopt;
  const auto size=std::uint32_t(byte(0)) | (std::uint32_t(byte(1))<<8U) |
      (std::uint32_t(byte(2))<<16U);
  if(size!=frame.size() || size<4U) return std::nullopt;

  // Validate the relocation traversal independently of the declaration
  // reader. Only payloads with no reference-bearing tags are supported.
  std::size_t walker=4U;
  bool terminated=false;
  while(walker<frame.size()) {
    const auto tag=byte(walker++);
    if(tag==0xffU) {terminated=true;break;}
    const auto kind=tag&0x3fU;
    if(kind==3U) {
      if(frame.size()-walker<4U) return std::nullopt;
      walker+=4U;
    } else if(kind==4U || kind==5U) {
      while(walker<frame.size() && byte(walker)!=0U) ++walker;
      if(walker==frame.size()) return std::nullopt;
      ++walker;
    } else return std::nullopt;
  }
  if(!terminated || frame.size()<6U || ((byte(4)&0x3fU)!=4U && (byte(4)&0x3fU)!=5U))
    return std::nullopt;
  const auto first=std::find(frame.begin()+5,frame.end(),std::byte{0});
  if(first==frame.end()) return std::nullopt;
  const auto first_end=static_cast<std::size_t>(first-frame.begin());
  if(frame.size()-first_end<3U) return std::nullopt;
  const auto declaration_tag=byte(first_end+1U)&0x3fU;
  if(declaration_tag!=4U && declaration_tag!=5U) return std::nullopt;
  const auto declaration_begin=first_end+2U;
  const auto second=std::find(frame.begin()+static_cast<std::ptrdiff_t>(declaration_begin),frame.end(),std::byte{0});
  if(second==frame.end()) return std::nullopt;
  const auto declaration_end=static_cast<std::size_t>(second-frame.begin());
  const std::string declarations(reinterpret_cast<const char*>(frame.data()+declaration_begin),
                                 declaration_end-declaration_begin);
  PostloadIntegerProfile result;
  result.source_text.assign(reinterpret_cast<const char*>(frame.data()+5U),first_end-5U);
  std::size_t cursor=declaration_end+1U, begin=0U;
  while(begin<declarations.size()) {
    const auto end=declarations.find(';',begin);
    // This profile rejects ignored trailing segments and empty names.
    if(end==std::string::npos) return std::nullopt;
    const auto part=std::string_view(declarations).substr(begin,end-begin);
    const auto space=part.rfind(' ');
    if(space==std::string_view::npos) return std::nullopt;
    auto name=part.substr(space+1U);
    if(const auto equals=name.rfind('=');equals!=std::string_view::npos) name=name.substr(0,equals);
    if(name.empty() || cursor>=frame.size()) return std::nullopt;
    const auto tag=byte(cursor++);
    if((tag&0x3fU)!=3U || (tag&0x40U)!=0U || frame.size()-cursor<5U) return std::nullopt;
    const auto bits=std::uint32_t(byte(cursor)) | (std::uint32_t(byte(cursor+1U))<<8U) |
        (std::uint32_t(byte(cursor+2U))<<16U) | (std::uint32_t(byte(cursor+3U))<<24U);
    cursor+=4U;
    if(byte(cursor)!=0xffU && (byte(cursor)&0x40U)!=0U) return std::nullopt;
    result.entries.push_back({std::string(name),bits});
    begin=end+1U;
  }
  if(result.entries.empty() || cursor>=frame.size() || byte(cursor)!=0xffU) return std::nullopt;
  return result;
}
}
