#include "off/ui/text_layout.hpp"

#include <cstdint>
#include <new>

namespace off::ui {
namespace {
struct Scalar { std::uint32_t value{}; std::size_t begin{}; std::size_t end{}; };
constexpr std::size_t maximum_scalars = 1U << 20U;
constexpr std::size_t maximum_input_bytes = 4U * 1024U * 1024U;
bool range(std::uint32_t x, std::uint32_t a, std::uint32_t b) { return x >= a && x <= b; }
bool control(std::uint32_t x) { return x <= 0x1f || range(x, 0x7f, 0x9f) || x == 0x2028 || x == 0x2029; }
bool extend(std::uint32_t x) {
  return range(x, 0x300, 0x36f) || range(x, 0x483, 0x489) || range(x, 0x591, 0x5bd) ||
      range(x, 0x5bf, 0x5bf) || range(x, 0x5c1, 0x5c2) || range(x, 0x610, 0x61a) ||
      range(x, 0x64b, 0x65f) || range(x, 0x6d6, 0x6dc) || range(x, 0x6df, 0x6e4) ||
      range(x, 0x6e7, 0x6e8) || range(x, 0x6ea, 0x6ed) || range(x, 0x730, 0x74a) ||
      range(x, 0x7a6, 0x7b0) || range(x, 0x7eb, 0x7f3) || range(x, 0x816, 0x819) ||
      range(x, 0x81b, 0x823) || range(x, 0x825, 0x827) || range(x, 0x829, 0x82d) ||
      range(x, 0x900, 0x902) || range(x, 0x93a, 0x93c) || range(x, 0x93e, 0x94d) ||
      range(x, 0x1ab0, 0x1aff) || range(x, 0x1dc0, 0x1dff) || range(x, 0x20d0, 0x20ff) ||
      range(x, 0xfe00, 0xfe0f) || range(x, 0xfe20, 0xfe2f) || range(x, 0x1f3fb, 0x1f3ff) ||
      range(x, 0xe0100, 0xe01ef);
}
bool spacing_mark(std::uint32_t x) { return range(x, 0x903, 0x903) || range(x, 0x93b, 0x93b) || range(x, 0x93e, 0x940) || range(x, 0x949, 0x94c) || range(x, 0x982, 0x983); }
bool prepend(std::uint32_t x) { return range(x, 0x600, 0x605) || x == 0x6dd || x == 0x70f || range(x, 0x890, 0x891) || x == 0x8e2; }
bool ri(std::uint32_t x) { return range(x, 0x1f1e6, 0x1f1ff); }
bool pictographic(std::uint32_t x) { return range(x, 0x1f000, 0x1faff) || range(x, 0x2600, 0x27bf); }
enum class Hangul { none, l, v, t, lv, lvt };
Hangul hangul(std::uint32_t x) { if (range(x,0x1100,0x115f)||range(x,0xa960,0xa97c)) return Hangul::l; if (range(x,0x1160,0x11a7)||range(x,0xd7b0,0xd7c6)) return Hangul::v; if (range(x,0x11a8,0x11ff)||range(x,0xd7cb,0xd7fb)) return Hangul::t; if (!range(x,0xac00,0xd7a3)) return Hangul::none; return ((x-0xac00)%28)==0 ? Hangul::lv : Hangul::lvt; }
bool rtl(std::uint32_t x) { return range(x,0x590,0x8ff) || range(x,0xfb1d,0xfdff) || range(x,0xfe70,0xfefc) || range(x,0x10800,0x10fff); }
bool ltr(std::uint32_t x) { return (range(x, 'A', 'Z') || range(x, 'a', 'z') || range(x, 0x00c0, 0x02af) || range(x, 0x370, 0x52f) || range(x, 0x900, 0x1fff)); }
bool decode(std::string_view s, std::vector<Scalar>& out) {
  for (std::size_t i{}; i < s.size();) { const auto begin=i; const auto lead=static_cast<unsigned char>(s[i++]); unsigned n{}; std::uint32_t cp{}; if(lead<=0x7f) { n=0; cp=lead; } else if(lead>=0xc2&&lead<=0xdf){n=1;cp=lead&0x1f;} else if(lead>=0xe0&&lead<=0xef){n=2;cp=lead&0xf;} else if(lead>=0xf0&&lead<=0xf4){n=3;cp=lead&7;} else return false; if(n>s.size()-i) return false; for(unsigned j{};j<n;++j){auto b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&0x3f);} const auto min=n==0?0U:n==1?0x80U:n==2?0x800U:0x10000U; if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||out.size()==maximum_scalars)return false; out.push_back({cp,begin,i}); }
  return true;
}
bool no_break(const std::vector<Scalar>& s, std::size_t i) { const auto a=s[i-1].value,b=s[i].value; if(a==0x0d&&b==0x0a)return true; if(control(a)||control(b))return false; const auto ha=hangul(a),hb=hangul(b); if(ha==Hangul::l&&(hb==Hangul::l||hb==Hangul::v||hb==Hangul::lv||hb==Hangul::lvt))return true; if((ha==Hangul::lv||ha==Hangul::v)&&(hb==Hangul::v||hb==Hangul::t))return true; if((ha==Hangul::lvt||ha==Hangul::t)&&hb==Hangul::t)return true; if(extend(b)||b==0x200d||spacing_mark(b)||prepend(a))return true; if(ri(a)&&ri(b)){std::size_t n{};for(std::size_t k=i;k>0&&ri(s[k-1].value);--k)++n;return (n%2U)==1U;} if(pictographic(b)&&a==0x200d){std::size_t k=i-1;while(k>0&&extend(s[k-1].value))--k;return k>0&&pictographic(s[k-1].value);} return false; }
}
std::optional<TextLayoutBoundary> make_text_layout_boundary(std::string_view utf8) noexcept {
  if (utf8.size() > maximum_input_bytes) return std::nullopt;
  try {
    std::vector<Scalar> scalars;
    // Every scalar uses at least one byte, so this is bounded by the checked
    // input length and never reserves an attacker-controlled unbounded size.
    scalars.reserve(utf8.size());
    if (!decode(utf8, scalars)) return std::nullopt;
    TextLayoutBoundary result;
    if (scalars.empty()) return result;
    std::size_t start{};
    for (std::size_t i{1}; i <= scalars.size(); ++i) {
      if (i == scalars.size() || !no_break(scalars, i)) {
        result.clusters.push_back({scalars[start].begin,
                                   scalars[i - 1].end - scalars[start].begin,
                                   i - start});
        start = i;
      }
    }
    for (const auto &scalar : scalars) {
      if (result.base_direction != TextBaseDirection::neutral) continue;
      if (rtl(scalar.value)) result.base_direction = TextBaseDirection::right_to_left;
      else if (ltr(scalar.value)) result.base_direction = TextBaseDirection::left_to_right;
    }
    return result;
  } catch (const std::bad_alloc &) {
    return std::nullopt;
  }
}
} // namespace off::ui
