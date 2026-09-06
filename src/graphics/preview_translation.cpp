#include "off/graphics/preview_translation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace off::graphics {
namespace {
// Every product of two finite binary32 values is an integer multiple of 2^-298.
// Nine words cover that whole range, including carries from three products.
// Fixed-width unsigned arithmetic makes the precision policy identical on
// platforms where C++ long double is binary64, extended80 or binary128.
struct Wide {
  std::array<std::uint64_t,9> words{};
  bool negative{};
  int top() const {
    for(int i=8;i>=0;--i)
      if(words[static_cast<std::size_t>(i)])
        return i*64+63-std::countl_zero(words[static_cast<std::size_t>(i)]);
    return -1;
  }
  bool bit(int n) const {
    return n>=0 && ((words[static_cast<std::size_t>(n/64)]>>(n%64))&1U)!=0;
  }
  void increment(int n) {
    auto i=static_cast<std::size_t>(n/64);
    std::uint64_t carry=std::uint64_t{1}<<(n%64);
    while(carry && i<words.size()) {
      const auto old=words[i];words[i]+=carry;carry=words[i]<old?1U:0U;++i;
    }
    if(carry) throw std::runtime_error("Preview extended arithmetic capacity exceeded");
  }
  void round_below(int cut) {
    if(cut<=0) return;
    bool sticky=false;
    for(int i=0;i<cut-1;++i) sticky=sticky || bit(i);
    const bool up=bit(cut-1) && (sticky || bit(cut));
    for(int i=0;i<cut;++i)
      words[static_cast<std::size_t>(i/64)]&=~(std::uint64_t{1}<<(i%64));
    if(up) increment(cut);
  }
};
struct Part {std::uint32_t mantissa;int exponent;bool negative;};
Part split(float value) {
  if(!std::isfinite(value)) throw std::runtime_error("Preview translation inputs must be finite");
  const auto bits=std::bit_cast<std::uint32_t>(value);
  const auto exponent=(bits>>23U)&255U;
  return {(bits&0x7fffffU)|(exponent?0x800000U:0U),
          exponent?static_cast<int>(exponent)-150:-149,(bits>>31U)!=0};
}
Wide product(float left,float right) {
  const auto a=split(left),b=split(right);
  const auto mantissa=std::uint64_t{a.mantissa}*b.mantissa;
  const int shift=a.exponent+b.exponent+298;
  Wide result;result.negative=a.negative!=b.negative;
  const auto index=static_cast<std::size_t>(shift/64);
  result.words[index]=mantissa<<(shift%64);
  if(shift%64) result.words[index+1]=mantissa>>(64-shift%64);
  return result;
}
int compare(const Wide& a,const Wide& b) {
  for(std::size_t i=a.words.size();i>0;) {
    --i;if(a.words[i]!=b.words[i]) return a.words[i]>b.words[i]?1:-1;
  }
  return 0;
}
Wide sum(Wide a,Wide b) {
  if(a.negative==b.negative) {
    std::uint64_t carry=0;
    for(std::size_t i=0;i<a.words.size();++i) {
      const auto initial=a.words[i];
      a.words[i]+=b.words[i];const bool first=a.words[i]<initial;
      const auto partial=a.words[i];
      a.words[i]+=carry;carry=(first || a.words[i]<partial)?1U:0U;
    }
    if(carry) throw std::runtime_error("Preview extended sum capacity exceeded");
  } else {
    const auto order=compare(a,b);
    if(order==0) return {}; // Exact cancellation, including opposite zeros.
    if(order<0) std::swap(a,b);
    std::uint64_t borrow=0;
    for(std::size_t i=0;i<a.words.size();++i) {
      const auto initial=a.words[i];
      a.words[i]-=b.words[i];const bool first=initial<b.words[i];
      const auto partial=a.words[i];
      a.words[i]-=borrow;borrow=(first || partial<borrow)?1U:0U;
    }
  }
  a.round_below(a.top()-63);
  return a;
}
float store(Wide value) {
  value.round_below(std::max(149,value.top()-23));
  const int top=value.top();
  if(top>425) throw std::runtime_error("Preview translation exceeds binary32 range");
  std::uint32_t bits=value.negative?0x80000000U:0U;
  if(top>=0) {
    const int shift=top<172?149:top-23;
    std::uint32_t mantissa=0;
    for(int i=0;i<24;++i) if(value.bit(shift+i)) mantissa|=std::uint32_t{1}<<i;
    bits|=top<172?mantissa:
      (static_cast<std::uint32_t>(top-171)<<23U)|(mantissa&0x7fffffU);
  }
  return std::bit_cast<float>(bits);
}
}
std::array<float,3> transform_preview_translation(
    const std::array<float,3>& local,const std::array<float,9>& basis) {
  std::array<float,3> result;
  for(std::size_t i=0;i<3;++i)
    result[i]=store(sum(sum(product(local[0],basis[6+i]),product(local[1],basis[3+i])),
                         product(local[2],basis[i])));
  return result;
}
} // namespace off::graphics
