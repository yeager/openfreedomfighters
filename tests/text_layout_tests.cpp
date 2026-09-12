#include "off/ui/text_layout.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace { void check(bool v, std::string_view m) { if(!v) { std::cerr << m << '\n'; std::exit(1); } } }
int main() {
  using off::ui::TextBaseDirection;
  const auto ascii=off::ui::make_text_layout_boundary("ABC"); check(ascii&&ascii->clusters.size()==3&&ascii->base_direction==TextBaseDirection::left_to_right,"ASCII scalar clusters");
  const auto combining=off::ui::make_text_layout_boundary("e\xCC\x81x"); check(combining&&combining->clusters.size()==2&&combining->clusters[0].byte_length==3&&combining->clusters[0].scalar_count==2,"combining sequence stays indivisible");
  const auto flag=off::ui::make_text_layout_boundary("\xF0\x9F\x87\xB8\xF0\x9F\x87\xAA"); check(flag&&flag->clusters.size()==1&&flag->clusters[0].scalar_count==2,"regional indicators pair");
  const auto zwj=off::ui::make_text_layout_boundary("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x9A\x80"); check(zwj&&zwj->clusters.size()==1,"emoji ZWJ sequence stays indivisible");
  const auto hebrew=off::ui::make_text_layout_boundary("\xD7\x90"); check(hebrew&&hebrew->base_direction==TextBaseDirection::right_to_left,"RTL base direction is detected");
  check(!off::ui::make_text_layout_boundary(std::string_view{"\xC3\x28",2}),"malformed UTF-8 rejects");
  std::string oversized(4U * 1024U * 1024U + 1U, 'x');
  check(!off::ui::make_text_layout_boundary(oversized), "oversized text rejects before allocation");
}
