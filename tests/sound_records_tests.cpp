#include "off/audio/sound_records.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace off::audio;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& f) {
  bool rejected = false;
  try { f(); } catch (const std::runtime_error&) { rejected = true; }
  check(rejected, "expected explicit rejection");
}
off::data::SoundDefinitionBank bank() {
  std::vector<std::byte> bytes(64);
  const auto word = [&](std::size_t offset, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes[offset+i]=std::byte((value>>(i*8))&255);
  };
  word(17,1); word(21,48); word(25,0xabcdef01);
  word(29,std::bit_cast<std::uint32_t>(3.25F));
  bytes[48]=std::byte{'x'};
  return off::data::SoundDefinitionBank::parse(bytes,bytes.size());
}
off::data::GmsIntroSoundOwnerPrefix source() {
  off::data::GmsIntroSoundOwnerPrefix s;
  s.authored_type=5; s.sound_definition_reference=17;
  s.cone_scalars={360,360,0,0}; s.gain_multiplier=100;
  s.range_input_scalar=1; s.enabled_option=1;
  return s;
}
}
int main() {
  try {
    const auto definitions=bank();
    SoundRecordRegistry registry;
    auto lease=registry.create(23);
    auto& record=lease.get();
    check(record.owner==23 && record.playback_state==3 && record.flags==0x86 &&
          record.range==10000 && record.derived_range==12800 && !record.active_source &&
          !record.position && !record.final_scalar && !record.fade_enabled,
          "allocation preserves known defaults and unassigned fields");
    check(registry.categories()[0].gain==1 && !registry.categories()[0].selected,
          "positive constructor gain does not imply selected category");
    rejects([&]{ (void)registry.prepare(lease.binding(),definitions,0); });
    record.active_source=0;
    check(!registry.prepare(lease.binding(),definitions,0),"assigned null source is not unassigned");
    registry.apply_source(record,source());
    check(record.type==6 && record.active_source==17 && record.range==100 &&
          record.derived_range==12800 && record.flags==0x86 && record.final_scalar==0,
          "source range is distance, with ordered gain conversion");
    auto modified=source(); modified.gain_multiplier=0; modified.cone_scalars[2]=7;
    registry.apply_source(record,modified);
    check(record.range==1 && record.derived_range==2 && record.source_scalars[2]==-7,
          "zero multiplier branch and positive cone C sign");
    modified.range_input_scalar=std::numeric_limits<float>::infinity();
    rejects([&]{registry.apply_source(record,modified);});
    SoundRecord foreign;
    rejects([&]{registry.apply_source(foreign,source());});
    registry.apply_source(record,source()); record.progress=2;
    registry.set_special_mode(true);
    rejects([&]{(void)registry.prepare(lease.binding(),definitions,42);});
    check(registry.prepared().empty(),"unsupported mode cannot admit record");
    registry.set_special_mode(false);
    check(registry.prepare(lease.binding(),definitions,42) && record.playback_state==7 &&
          record.start_time==42 && record.duration==3.25F && record.progress==2 &&
          record.fade_enabled==false && record.fade_values==std::array<float,3>{0,0,0},
          "prepare retains duration without fabricating progress or playback acknowledgment");
    check(!registry.prepare(lease.binding(),definitions,43),"prepare requires state three");
    auto second=registry.create(24); registry.apply_source(second.get(),source());
    registry.categories()[0].selected=true;
    check(registry.prepare(second.binding(),definitions,44) && second.get().playback_state==10,
          "prepare uses canonical live category selection");
    record.playback_state=3;
    check(registry.prepare(lease.binding(),definitions,45),"duplicate prepared entry accepted");
    registry.stop(lease.binding());
    check(registry.prepared().size()==2 && registry.prepared()[0]==lease.binding() &&
          registry.prepared()[1]==second.binding() && registry.pending_stops().size()==1 &&
          record.playback_state==3,"stop removes first occurrence with swap-last ordering");
    registry.stop(lease.binding());
    check(registry.pending_stops().size()==2 && registry.prepared().size()==1,
          "pending stops retain duplicate requests");
    record.playback_state=12; registry.stop(lease.binding());
    check(record.playback_state==12 && registry.pending_stops().size()==3,
          "unlisted state twelve queues without resetting state");
    // Explicit receiver fixtures: these calls do not stand in for a native audio producer.
    record.start_time=0xfffffe00U; record.duration=3.25F; record.seek=.25F;
    registry.acknowledge_started(lease.binding(),0x200U);
    check(record.progress==1 && record.duration==4,"raw clock wrap and seek arithmetic");
    record.start_time=100; record.duration=3.25F; record.seek=0;
    registry.acknowledge_started(lease.binding(),100);
    check(record.progress==0 && record.duration==3.25F,"zero elapsed acknowledgment remains zero");
    const auto stale=lease.binding();
    SoundRecordLease moved=std::move(lease);
    check(lease.binding()==0 && moved.binding()==stale,"lease move transfers single ownership");
    rejects([&]{(void)lease.get();});
    moved.reset();
    check(!registry.resolve(stale) && registry.pending_stops().empty(),"owner teardown purges every queued occurrence");
    auto replacement=registry.create(25);
    check(replacement.binding()>stale,"native bindings are not reused");
    registry.acknowledge_started(stale,1234);
    check(replacement.get().progress==0,"stale acknowledgment cannot retarget another scene owner");
    SoundRecord* stable=&replacement.get();
    std::vector<SoundRecordLease> pool;
    while(registry.size()<SoundRecordRegistry::capacity) pool.push_back(registry.create(1));
    check(&replacement.get()==stable,"record addresses remain stable across pool growth");
    rejects([&]{(void)registry.create(1);});
    pool.clear();
    check(registry.size()==2,"scene lease cleanup returns capacity without releasing other owners");
    rejects([&]{(void)registry.create(0);});
    std::cout<<"Canonical sound records, source state, bounded preparation, queue semantics and stale acknowledgments verified.\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
