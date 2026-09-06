#include "off/audio/sound_records.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::audio {
namespace {
float binary32(double value) {
  if (!std::isfinite(value) || value>std::numeric_limits<float>::max() ||
      value<-std::numeric_limits<float>::max())
    throw std::runtime_error("Sound record arithmetic exceeds finite binary32 range");
  volatile float result=static_cast<float>(value);
  return result;
}
}
SoundRecordLease::~SoundRecordLease() { reset(); }
SoundRecordLease::SoundRecordLease(SoundRecordLease&& other) noexcept
  : registry_(std::exchange(other.registry_,nullptr)),binding_(std::exchange(other.binding_,0)) {}
SoundRecordLease& SoundRecordLease::operator=(SoundRecordLease&& other) noexcept {
  if (this!=&other) {
    reset(); registry_=std::exchange(other.registry_,nullptr);
    binding_=std::exchange(other.binding_,0);
  }
  return *this;
}
SoundRecord& SoundRecordLease::get() const {
  if (!registry_) throw std::runtime_error("Sound record lease is empty");
  auto* record=registry_->resolve(binding_);
  if (!record) throw std::runtime_error("Sound record lease is no longer live");
  return *record;
}
void SoundRecordLease::reset() noexcept {
  if (registry_) registry_->release(binding_);
  registry_=nullptr; binding_=0;
}
SoundRecordRegistry::SoundRecordRegistry() {
  categories_[0].gain=1; categories_[1].gain=0.2F;
  categories_[2].gain=0.95F; categories_[3].gain=0.85F;
}
SoundRecordLease SoundRecordRegistry::create(std::uint64_t owner) {
  if (!owner || records_.size()>=capacity || next_binding_==0)
    throw std::runtime_error("Sound record owner, capacity or binding domain is unavailable");
  auto record=std::make_unique<SoundRecord>();
  const auto binding=next_binding_;
  record->binding=binding; record->owner=owner;
  records_.emplace(binding,std::move(record));
  ++next_binding_; // Zero after the final nonzero binding is an exhaustion marker.
  return SoundRecordLease(*this,binding);
}
SoundRecord* SoundRecordRegistry::resolve(std::uint64_t binding) noexcept {
  const auto found=records_.find(binding);
  return found==records_.end()?nullptr:found->second.get();
}
const SoundRecord* SoundRecordRegistry::resolve(std::uint64_t binding) const noexcept {
  const auto found=records_.find(binding);
  return found==records_.end()?nullptr:found->second.get();
}
void SoundRecordRegistry::release(std::uint64_t binding) noexcept {
  const auto found=records_.find(binding);
  if (found==records_.end()) return;
  found->second->owner=0;
  // Whole-owner native teardown invalidates all queued occurrences. This is not
  // the original single-occurrence stop operation or device resource disposal.
  std::erase(prepared_,binding); std::erase(pending_stops_,binding);
  records_.erase(found);
}
void SoundRecordRegistry::apply_source(SoundRecord& record,const data::GmsIntroSoundOwnerPrefix& source) {
  if (resolve(record.binding)!=&record)
    throw std::runtime_error("Sound source requires this registry's live record");
  if (source.authored_type>9 || source.category>=categories_.size())
    throw std::runtime_error("Sound source type or category is unsupported");
  for (float value:source.cone_scalars)
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite sound source scalar");
  for (float value:{source.gain_multiplier,source.range_input_scalar,source.final_scalar})
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite sound source parameter");
  record.type=source.authored_type+1;
  record.active_source=source.sound_definition_reference;
  record.source_scalars=source.cone_scalars;
  record.flags=source.loop_option?(record.flags|8U):(record.flags&~8U);
  record.gain_multiplier=source.gain_multiplier;
  float range=1,derived=2;
  if (record.gain_multiplier!=0) {
    const float quotient=binary32(double(record.gain_multiplier)/double(0.78125F));
    range=binary32(double(source.range_input_scalar)*double(100.0F));
    derived=binary32(double(quotient)*double(range));
  }
  record.range=range; record.derived_range=derived;
  record.flags|=0x80U; record.category=source.category;
  record.flags=source.enabled_option?(record.flags|2U):(record.flags&~2U);
  if (record.source_scalars[2]>0) record.source_scalars[2]=-record.source_scalars[2];
  record.final_scalar=source.final_scalar;
}
bool SoundRecordRegistry::prepare(std::uint64_t binding,const data::SoundDefinitionBank& bank,std::uint32_t raw_time) {
  if (special_mode_) throw std::runtime_error("Additional sound preparation mode is unsupported");
  auto* record=resolve(binding);
  if (!record || record->playback_state!=3) return false;
  if (!record->active_source) throw std::runtime_error("Sound preparation requires assigned source token");
  const auto definition=bank.simple_definition(*record->active_source);
  if (!definition) return false;
  record->output_mode=2;
  record->fade_enabled=false; record->fade_values=std::array<float,3>{0,0,0};
  record->active_source=definition->definition_offset;
  record->start_time=raw_time; record->duration=definition->duration;
  if (record->category>=categories_.size()) throw std::runtime_error("Sound preparation category is unsupported");
  record->playback_state=categories_[record->category].selected?10U:7U;
  prepared_.push_back(binding);
  return true;
}
void SoundRecordRegistry::stop(std::uint64_t binding) {
  const auto found=std::find(prepared_.begin(),prepared_.end(),binding);
  if (found!=prepared_.end()) {
    pending_stops_.push_back(binding);
    *found=prepared_.back(); prepared_.pop_back();
    if (auto* record=resolve(binding)) record->playback_state=3;
  } else if (const auto* record=resolve(binding); record && record->playback_state==12) {
    pending_stops_.push_back(binding);
  }
}
void SoundRecordRegistry::acknowledge_started(std::uint64_t binding,std::uint32_t raw_time) {
  auto* record=resolve(binding);
  if (!record) return;
  const auto difference=std::bit_cast<std::int32_t>(raw_time-record->start_time);
  const float elapsed=binary32(double(binary32(double(difference)))*double(1.0F/1024.0F));
  record->progress=elapsed;
  const float sum=binary32(double(record->duration)+double(elapsed));
  record->duration=binary32(double(sum)-double(record->seek));
}
} // namespace off::audio
