#include "off/audio/sound_records.hpp"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
  bool rejected=false; try { f(); } catch (const std::runtime_error&) { rejected=true; }
  check(rejected,"expected category request rejection");
}
float rounded(float value) { volatile float stored=value; return stored; }
}
int main() {
  try {
    off::audio::SoundRecordRegistry backend;
    check(backend.categories()[0].gain==1 && backend.categories()[1].gain==0.2F &&
          backend.categories()[2].gain==0.95F && backend.categories()[3].gain==0.85F,
          "cold gains are not category volume multipliers");
    for (const auto& category:backend.categories())
      check(!category.selected && category.adjustment==0,"cold categories remain unselected");
    check(!backend.pending_volume_update(),"no invented initial volume request");
    rejects([&] { backend.request_category_volume(8,90,2); });
    rejects([&] { backend.request_category_volume(0,90,1); });
    check(!backend.pending_volume_update() && backend.categories()[0].gain==1,
          "unsupported request has no mutations");
    backend.request_category_volume(0,90,2);
    check(backend.categories()[0].gain==rounded(rounded(74.99F*0.01F)*0.56F) &&
          backend.categories()[0].selected && backend.pending_volume_update(),
          "nonlinear integer response and separate binary32 multiplies");
    const float endpoints[]{0,0.18F,0.22F,0.26F,0.32F};
    for (int i=0;i<5;++i) {
      backend.request_category_volume(3,i,2);
      check(backend.categories()[3].gain==rounded(endpoints[i]*0.01F),"low-volume responses");
    }
    float previous=-1;
    for (int volume=0;volume<=100;++volume) {
      backend.request_category_volume(7,volume,2);
      const auto gain=backend.categories()[7].gain;
      check(gain>=previous && std::isfinite(gain),"supported integer response is monotonic and finite");
      check(backend.categories()[7].selected==(volume>0),"selection follows computed gain");
      previous=gain;
    }
    for (std::uint32_t category=0;category<8;++category) {
      backend.request_category_volume(category,100,2);
      const float expected=category==0?0.56F:category==1?0.49F:category==2?0.89F:1.0F;
      check(backend.categories()[category].gain==expected,"category multipliers and eight-entry capacity");
    }
    for (auto volume:{-1,101,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}) {
      backend.request_category_volume(6,volume,2);
      check(backend.categories()[6].gain==rounded(static_cast<float>(volume)*0.01F),
            "out-of-range signed input bypasses response without clamping");
    }
    off::audio::SoundRecordRegistry live;
    std::vector<std::byte> bytes(32);
    bytes[16]=std::byte{1};
    const auto duration=std::bit_cast<std::uint32_t>(9.25F);
    for (unsigned i=0;i<4;++i) bytes[28+i]=static_cast<std::byte>((duration>>(8*i))&255);
    const auto bank=off::data::SoundDefinitionBank::parse(bytes,32);
    auto first=live.create(19), other=live.create(23);
    first.get().active_source=16; other.get().active_source=16; other.get().category=1;
    check(live.prepare(first.binding(),bank,100) && live.prepare(other.binding(),bank,100),
          "prepare source-backed records before volume traversal");
    check(first.get().playback_state==7,"positive cold gain does not select prepared record");
    live.request_category_volume(0,90,2);
    check(first.get().playback_state==5 && other.get().playback_state==7 &&
          first.get().duration==9.25F && first.get().progress==0 && live.prepared().size()==2,
          "positive transition changes matching canonical states, not duration/progress/list");
    first.get().playback_state=12;
    live.request_category_volume(0,90,2);
    check(first.get().playback_state==12,"already selected category skips state traversal");
    live.request_category_volume(0,0,2);
    live.set_special_mode(true);
    rejects([&] { (void)live.prepare(first.binding(),bank,200); });
    live.request_category_volume(0,100,2);
    check(first.get().playback_state==12 && live.categories()[0].selected,
          "suppression skips record writes but not gain/selection");
    live.request_category_volume(0,-1,2);
    check(!live.categories()[0].selected && first.get().playback_state==12,
          "nonpositive result clears selection without state traversal");
    live.set_special_mode(false);
    live.request_category_volume(0,1,2);
    check(first.get().playback_state==5,"new positive transition uses current registry state");
  } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
