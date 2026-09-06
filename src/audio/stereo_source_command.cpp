#include "off/audio/stereo_source_command.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::audio {
namespace {
float f(float value) {
  volatile float result=value;
  if(!std::isfinite(result)) throw std::runtime_error("stereo command arithmetic is not finite");
  return result;
}
std::int32_t integer(float value) {
  const double truncated=std::trunc(double(value));
  if(!std::isfinite(truncated) || truncated<double(std::numeric_limits<std::int32_t>::min()) ||
     truncated>double(std::numeric_limits<std::int32_t>::max()))
    throw std::runtime_error("stereo command integer conversion is not representable");
  return static_cast<std::int32_t>(truncated);
}
void supported(const SoundRecord& record) {
  if(record.type!=6 || record.output_mode!=2)
    throw std::runtime_error("source is outside the intro stereo command path");
}
}
float weight_intro_stereo_source(SoundRecord& record) {
  supported(record);
  const float weight=f(record.gain*100.0F);
  record.priority=integer(f(weight*10.0F));
  return weight;
}
StereoCommandBatch build_intro_stereo_commands(SoundRecordRegistry& registry,
    const data::SoundDefinitionBank& bank,std::span<const std::uint64_t> ordered) {
  StereoCommandBatch batch;
  for(const auto binding:ordered.first(std::min<std::size_t>(65,ordered.size()))) {
    ++batch.visited;
    auto* record=registry.resolve(binding);
    if(!record) { batch.diagnostics.push_back("Stereo source binding is no longer live"); continue; }
    supported(*record);
    if(!record->active_source) throw std::runtime_error("stereo source is unassigned");
    const auto definition=bank.simple_definition(*record->active_source);
    if(!definition) { batch.diagnostics.push_back("Stereo source has no simple SND entry"); continue; }
    const auto whd=definition->resource_link&~std::uint32_t{1};
    if(!whd) { batch.diagnostics.push_back("Stereo source has no WHD reference"); continue; }
    if(!record->priority || record->category>=registry.categories().size())
      throw std::runtime_error("stereo source has no admitted priority or valid category");
    const auto& category=registry.categories()[record->category];
    const auto sum=std::int64_t(category.adjustment)+record->timing_changes[1];
    if(sum<std::numeric_limits<std::int32_t>::min() || sum>std::numeric_limits<std::int32_t>::max())
      throw std::runtime_error("stereo frequency adjustment sum is not representable");
    if(record->grouping_count>0 && !record->environment_group_index)
      throw std::runtime_error("stereo source grouping index is unassigned");
    const float gain=f(f(category.gain*registry.master_gain())*f(record->gain_multiplier*record->gain));
    const bool start=record->playback_state==5;
    if(start) record->playback_state=1;
    batch.sources.push_back({binding,start,(record->flags&8U)!=0,*record->priority,whd,
      record->grouping_count>0?*record->environment_group_index:-1,record->pan,
      static_cast<std::int32_t>(sum),gain});
  }
  return batch;
}
StereoOutputControls stereo_output_controls(float gain,std::int32_t pan,
    std::int32_t adjustment,std::uint32_t sample_rate) {
  if(!sample_rate || !std::isfinite(gain)) throw std::runtime_error("stereo controls require finite gain and sample rate");
  if(gain<=0.001F) gain=0.0F;
  else if(gain>100.0F) gain=100.0F;
  std::int32_t volume=-10000;
  if(gain!=0) {
    const float t=f(static_cast<float>(double(gain)*0.01));
    volume=integer(f(static_cast<float>(std::log10(double(t))*2000.0)));
  }
  const float rate=f(static_cast<float>(sample_rate));
  const float q=f(f(static_cast<float>(adjustment))/1200.0F);
  const float frequency=adjustment<0?f(f(1.0F/f(1.0F-q))*rate):f(rate*f(q+1.0F));
  return {gain,std::clamp(pan,-10000,10000),volume,
    static_cast<std::uint32_t>(std::clamp(integer(frequency),100,100000))};
}
} // namespace off::audio
