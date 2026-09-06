#include "off/audio/stereo_source_command.hpp"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
template<class F> void rejects(F action) {
  bool rejected=false; try { action(); } catch(const std::runtime_error&) { rejected=true; }
  check(rejected,"expected explicit stereo command failure");
}
void put(std::vector<std::byte>& bytes,std::size_t offset,std::uint32_t value) {
  for(unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<std::byte>((value>>(8*i))&255);
}
}
int main() {
  try {
    std::vector<std::byte> bytes(80);
    for(const auto offset:{16U,32U}) {
      put(bytes,offset,1); put(bytes,offset+4,72);
      put(bytes,offset+8,offset==16?17:1); put(bytes,offset+12,std::bit_cast<std::uint32_t>(9.0F));
    }
    bytes[72]=std::byte{'x'};
    const auto bank=off::data::SoundDefinitionBank::parse(bytes,80);
    off::audio::SoundRecordRegistry registry;
    auto lease=registry.create(123);
    auto& record=lease.get();
    check(!record.priority && !record.environment_group_index && record.pan==0 &&
          record.grouping_count==0 && registry.master_gain()==1,"known defaults do not invent producer outputs");
    record.type=6; record.active_source=16; record.playback_state=5;
    const std::array bindings{lease.binding()};
    rejects([&] { (void)off::audio::build_intro_stereo_commands(registry,bank,bindings); });
    check(record.playback_state==5,"unweighted source rejects before start transition");
    check(off::audio::weight_intro_stereo_source(record)==100 && record.priority==1000,
          "priority uses gain weight, not category or final output gain");
    registry.categories()[0].gain=0.5F;
    registry.categories()[0].adjustment=100;
    registry.set_master_volume(25);
    record.gain_multiplier=2; record.timing_changes={777,-1200}; record.pan=-12345;
    record.flags|=8;
    auto batch=off::audio::build_intro_stereo_commands(registry,bank,bindings);
    const auto command=batch.sources.at(0);
    check(command.start_requested && command.loop && command.priority==1000 && command.whd_offset==16 &&
          command.environment_group==-1 && command.pan==-12345 && command.frequency_adjustment==-1100 &&
          command.gain==0.25F && record.playback_state==1 && record.progress==0 && record.duration==0,
          "semantic command reads canonical fields without a playback acknowledgement");
    check(!off::audio::build_intro_stereo_commands(registry,bank,bindings).sources[0].start_requested,
          "only state five produces a start request");
    record.gain=0; record.playback_state=5; record.grouping_count=-1;
    check(off::audio::weight_intro_stereo_source(record)==0,"muted records still receive priority");
    batch=off::audio::build_intro_stereo_commands(registry,bank,bindings);
    check(batch.sources.size()==1 && batch.sources[0].start_requested && batch.sources[0].gain==0 &&
          batch.sources[0].priority==0 && batch.sources[0].environment_group==-1,
          "muted start is retained; negative grouping count does not read an unassigned index");
    record.grouping_count=1;
    rejects([&] { (void)off::audio::build_intro_stereo_commands(registry,bank,bindings); });
    record.environment_group_index=7;
    check(off::audio::build_intro_stereo_commands(registry,bank,bindings).sources[0].environment_group==7,
          "positive grouping count consumes the retained index");
    record.grouping_count=0;
    registry.categories()[0].adjustment=std::numeric_limits<std::int32_t>::max();
    record.timing_changes[1]=1;
    rejects([&] { (void)off::audio::build_intro_stereo_commands(registry,bank,bindings); });
    registry.categories()[0].adjustment=0; record.timing_changes[1]=0;
    std::array<std::uint64_t,66> visits{}; visits[65]=lease.binding();
    batch=off::audio::build_intro_stereo_commands(registry,bank,visits);
    check(batch.visited==65 && batch.sources.empty() && batch.diagnostics.size()==65,
          "skipped entries consume the 65-visit budget");
    for(const auto source:{0U,32U}) {
      record.active_source=source; record.playback_state=5;
      batch=off::audio::build_intro_stereo_commands(registry,bank,bindings);
      check(batch.sources.empty() && batch.diagnostics.size()==1 && record.playback_state==5,
            "missing SND/WHD source does not invent a start command");
    }
    registry.set_master_volume(-50);
    check(registry.master_gain()==-0.5F && !registry.pending_volume_update(),
          "master percent setter has no category curve, clamp or invented pending update");
    const auto full=off::audio::stereo_output_controls(100,20000,0,44100);
    check(full.retained_gain==100 && full.volume_hundredths_db==0 && full.pan==10000 && full.frequency_hz==44100,
          "actual neutral frequency and full gain controls");
    for(const auto gain:{-10.0F,-0.0F,0.0F,0.001F}) {
      const auto controls=off::audio::stereo_output_controls(gain,-20000,0,44100);
      check(controls.retained_gain==0 && !std::signbit(controls.retained_gain) &&
            controls.volume_hundredths_db==-10000 && controls.pan==-10000,"threshold clamps retained gain to positive zero");
    }
    check(off::audio::stereo_output_controls(101,0,0,44100).retained_gain==100 &&
          off::audio::stereo_output_controls(1,0,0,44100).volume_hundredths_db==-4000,
          "channel logarithmic volume is separate from category response");
    check(off::audio::stereo_output_controls(1,0,1200,44100).frequency_hz==88200 &&
          off::audio::stereo_output_controls(1,0,-1200,44100).frequency_hz==22050 &&
          off::audio::stereo_output_controls(1,0,2400,44100).frequency_hz==100000 &&
          off::audio::stereo_output_controls(1,0,std::numeric_limits<std::int32_t>::min(),44100).frequency_hz==100,
          "ordered signed frequency adjustment and integer clamp");
    rejects([&] { (void)off::audio::stereo_output_controls(1,0,std::numeric_limits<std::int32_t>::max(),44100); });
    rejects([&] { (void)off::audio::stereo_output_controls(std::numeric_limits<float>::quiet_NaN(),0,0,44100); });
    rejects([&] { (void)off::audio::stereo_output_controls(1,0,0,0); });
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
