#include "off/graphics/preview_translation.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F action) {
  bool rejected=false;try{action();}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"Expected invalid translation rejection");
}
std::array<float,9> basis(float x,float y,float z) {return {z,z,z,y,y,y,x,x,x};}
void equals(const std::array<float,3>& local,const std::array<float,9>& frame,float expected) {
  for(float result:off::graphics::transform_preview_translation(local,frame))
    check(std::bit_cast<std::uint32_t>(result)==std::bit_cast<std::uint32_t>(expected),"Translation bit pattern differs");
}
}
int main() {
 try {
  using off::graphics::transform_preview_translation;
  equals({2,3,5},basis(7,11,13),112);
  check(transform_preview_translation({2,3,5},{1,2,3,4,5,6,7,8,9})==
        std::array<float,3>{31,41,51},"Nonuniform basis preserves all coordinate mappings");
  const float huge=0x1p64F;
  // Deliberately distinguishes retained 64-bit significand rounding from
  // exact accumulation or a binary128 platform long double.
  equals({1,1,1},basis(huge,1,-huge),0);
  equals({1,1,1},basis(huge,3,-huge),4);
  equals({1,1,1},basis(-huge,-3,huge),-4);
  equals({1,1,0},basis(1,0x1p-24F,0),1);
  equals({1,3,0},basis(1,0x1p-24F,0),1+0x1p-22F);
  const auto tiny=std::numeric_limits<float>::denorm_min();
  equals({tiny,0,0},basis(0.5F,0,0),0);
  equals({tiny,0,0},basis(1.5F,0,0),2*tiny);
  equals({0,0,0},basis(-1,-1,-1),-0.0F);
  equals({-0.0F,-0.0F,0},basis(1,1,1),0);
  equals({-tiny,-0.0F,-0.0F},basis(0.5F,1,1),-0.0F);
  const auto maximum=std::numeric_limits<float>::max();
  equals({maximum,maximum,1},basis(maximum,-maximum,-3),-3);
  rejects([&]{(void)transform_preview_translation({maximum,0,0},basis(maximum,0,0));});
  for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    rejects([&]{(void)transform_preview_translation({bad,0,0},basis(0,0,0));});
    rejects([&]{(void)transform_preview_translation({0,0,0},basis(bad,0,0));});
  }
  // Independent native extended80 oracle where available. Other platforms
  // execute the fixed adversarial vectors, never a binary64/128 substitute.
  if constexpr(std::numeric_limits<long double>::digits==64) {
    std::uint32_t rng=0x53714581U;
    const auto sample=[&]() {
      rng^=rng<<13U;rng^=rng>>17U;rng^=rng<<5U;
      auto bits=rng;if((bits&0x7f800000U)==0x7f800000U) bits^=0x00800000U;
      return std::bit_cast<float>(bits);
    };
    for(unsigned iteration=0;iteration<12000;++iteration) {
      std::array<float,3> local;std::array<float,9> frame;
      for(auto& value:local) value=sample();
      for(auto& value:frame) value=sample();
      std::array<float,3> expected;
      bool overflow=false;
      for(std::size_t i=0;i<3;++i) {
        const volatile long double a=static_cast<long double>(local[0])*frame[6+i];
        const volatile long double b=static_cast<long double>(local[1])*frame[3+i];
        const volatile long double c=static_cast<long double>(local[2])*frame[i];
        const volatile long double partial=a+b;
        const volatile long double total=partial+c;
        const volatile float stored=static_cast<float>(total);
        expected[i]=stored;overflow=overflow || !std::isfinite(stored);
      }
      if(overflow) {rejects([&]{(void)transform_preview_translation(local,frame);});continue;}
      const auto actual=transform_preview_translation(local,frame);
      for(std::size_t i=0;i<3;++i)
        check(std::bit_cast<std::uint32_t>(actual[i])==std::bit_cast<std::uint32_t>(expected[i]),
              "Portable translation disagrees with independent extended80 arithmetic");
    }
  }
  std::cout<<"Portable preview translation precision verified.\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
