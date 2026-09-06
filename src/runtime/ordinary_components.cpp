#include "off/runtime/ordinary_components.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace off::runtime {
void OrdinarySortingState::sort(std::span<OrdinarySortItem> items) {
  if(items.size()>OrdinaryComponentManager::pending_capacity)
    throw std::runtime_error("Ordinary sorting exceeds pending capacity");
  const auto partition=[&](auto&& self,std::ptrdiff_t lo,std::ptrdiff_t hi)->void {
    if(lo>=hi) return;
    state=state+std::rotl(state,static_cast<int>(state&31U))+3U;
    const auto pivot_index=lo+static_cast<std::ptrdiff_t>((state&0x00ffffffU)%static_cast<std::uint32_t>(hi-lo+1));
    const auto pivot=items[static_cast<std::size_t>(pivot_index)].key;
    auto left=lo,right=hi;
    while(left<=right) {
      while(left<=hi && items[static_cast<std::size_t>(left)].key<pivot) ++left;
      while(right>=lo && items[static_cast<std::size_t>(right)].key>pivot) --right;
      if(left<=right) {std::swap(items[static_cast<std::size_t>(left)],items[static_cast<std::size_t>(right)]);++left;--right;}
    }
    if(lo<right) self(self,lo,right);
    if(left<hi) self(self,left,hi);
  };
  if(items.size()>1) partition(partition,0,static_cast<std::ptrdiff_t>(items.size()-1));
}
OrdinaryComponentManager::OrdinaryComponentManager(OrdinarySortingState& sorting,OrdinaryMembershipServices services)
  :sorting_(sorting),services_(std::move(services)) {
  if(!services_.resolve_component) throw std::runtime_error("Ordinary membership requires a component resolver");
  retained_.reserve(retained_capacity);pending_.reserve(pending_capacity);
}
void OrdinaryComponentManager::check_idle() const {
  if(failed_ || refreshing_ || traversing_) throw std::runtime_error("Ordinary manager is busy or failed");
}
ComponentRecord* OrdinaryComponentManager::resolve(std::uint64_t handle) const {
  auto* record=services_.resolve_component(handle);
  if(record && (!record->constructed() || record->removed())) return nullptr;
  return record;
}
ComponentRecord& OrdinaryComponentManager::require_live(std::uint64_t handle) const {
  auto* result=resolve(handle);
  if(!result) throw std::runtime_error("Ordinary component membership changed during refresh");
  return *result;
}
std::optional<OrdinaryOwner> OrdinaryComponentManager::owner(ComponentRecord& record) const {
  const auto& s=record.state();
  if(s.attached_owner) {
    if(!services_.attached_owner) throw std::runtime_error("Ordinary attached owner service missing");
    if(auto found=services_.attached_owner(s.attached_owner)) return found;
  }
  if(s.script_reference) {
    if(!services_.script_owner) throw std::runtime_error("Ordinary script owner service missing");
    return services_.script_owner(s.script_reference);
  }
  return std::nullopt;
}
std::pair<std::uint32_t,std::uint32_t> OrdinaryComponentManager::key(ComponentRecord& record) const {
  const auto& s=record.state();
  if(s.priority==0xffffffffU || !record.identity())
    throw std::runtime_error("Ordinary priority or component identity is unsupported");
  std::uint32_t ordinal=s.class_ordinal;
  if(s.script_reference) {
    const auto resolved=owner(record);
    if(resolved && !resolved->class_ordinal) throw std::runtime_error("Ordinary owner class ordinal is unavailable");
    ordinal=resolved?*resolved->class_ordinal:0;
  }
  return {s.priority,(ordinal<<20U)|*record.identity()};
}
void OrdinaryComponentManager::compact() {
  for(std::size_t i=0;i<retained_.size();) {
    auto* r=resolve(retained_[i]);
    bool remove=!r;
    if(r && !(r->state().admitted&0x10U) && (r->state().registered_cache&0x10U)) {
      r->state().registered_cache&=~0x10U;remove=true;
    }
    if(remove) {
      retained_keys_.erase(retained_[i]);
      retained_.erase(retained_.begin()+static_cast<std::ptrdiff_t>(i));
    }
    else ++i;
  }
}
void OrdinaryComponentManager::enqueue(std::uint64_t handle) {
  if(failed_ || refreshing_ || !handle) throw std::runtime_error("Ordinary pending addition is inadmissible");
  if(pending_.size()==pending_capacity) {
    if(traversing_) throw std::runtime_error("Ordinary pending overflow during traversal");
    refresh();
  }
  pending_.push_back(handle);
}
void OrdinaryComponentManager::notify_removal() {
  if(failed_ || refreshing_ || removals_==std::numeric_limits<std::size_t>::max())
    throw std::runtime_error("Ordinary removal notification is inadmissible");
  ++removals_;
  if(removals_>200 && !traversing_) {
    refreshing_=true;
    try {compact();removals_=0;refreshing_=false;}
    catch(...) {refreshing_=false;failed_=true;throw;}
  }
}
void OrdinaryComponentManager::refresh() {
  check_idle();refreshing_=true;
  try {
    compact();
    for(const auto handle:retained_) {
      if(key(require_live(handle))!=retained_keys_.at(handle))
        throw std::runtime_error("Ordinary retained ordering key changed; remove and reenroll first");
    }
    for(std::size_t i=0;i<pending_.size();) {
      if(!resolve(pending_[i])) pending_.erase(pending_.begin()+static_cast<std::ptrdiff_t>(i));
      else ++i;
    }
    std::vector<OrdinarySortItem> sorted;sorted.reserve(pending_.size());
    for(const auto h:pending_) sorted.push_back({h,key(require_live(h)).first});
    sorting_.sort(sorted);
    for(std::size_t begin=0;begin<sorted.size();) {
      auto end=begin+1;
      while(end<sorted.size() && sorted[end].key==sorted[begin].key) ++end;
      for(auto i=begin;i<end;++i) sorted[i].key=key(require_live(sorted[i].handle)).second;
      sorting_.sort(std::span(sorted).subspan(begin,end-begin));begin=end;
    }
    std::vector<std::uint64_t> merged;merged.reserve(retained_capacity);
    const auto append=[&](std::uint64_t handle) {
      if(merged.size()==retained_capacity) throw std::runtime_error("Ordinary retained capacity exhausted");
      merged.push_back(handle);
    };
    std::size_t p=0,r=0;
    while(p<sorted.size() || r<retained_.size()) {
      if(p<sorted.size() && (r==retained_.size() ||
          key(require_live(sorted[p].handle))<key(require_live(retained_[r])))) {
        const auto h=sorted[p++].handle;auto& s=require_live(h).state();
        if((s.admitted&0x10U) && !(s.registered_cache&0x10U)) {append(h);s.registered_cache|=0x10U;}
      } else append(retained_[r++]);
    }
    std::map<std::uint64_t,std::pair<std::uint32_t,std::uint32_t>> merged_keys;
    for(const auto handle:merged) merged_keys.emplace(handle,key(require_live(handle)));
    retained_=std::move(merged);retained_keys_=std::move(merged_keys);pending_.clear();refreshing_=false;
  } catch(...) {refreshing_=false;failed_=true;throw;}
}
std::span<const std::uint64_t> OrdinaryComponentManager::retained() const {
  if(refreshing_) throw std::runtime_error("Ordinary retained snapshot during refresh");
  return retained_;
}
std::span<const std::uint64_t> OrdinaryComponentManager::pending() const {
  if(refreshing_) throw std::runtime_error("Ordinary pending snapshot during refresh");
  return pending_;
}
void OrdinaryComponentManager::dispatch(const OrdinaryDispatchServices& supplied) {
  check_idle();
  const auto services=supplied;
  if(!services.scene_integer || !services.assign_dispatch_time || !services.paused || !services.filter)
    throw std::runtime_error("Ordinary dispatch requires clock, pause and filter services");
  refresh();traversing_=true;
  try {
    services.assign_dispatch_time(services.scene_integer());
    const bool paused=services.paused();const auto filter=services.filter();
    for(std::size_t i=0;i<retained_.size();++i) {
      const auto handle=retained_[i];auto* record=resolve(handle);
      if(!record || !(record->state().admitted&0x10U)) continue;
      const auto resolved_owner=owner(*record);
      auto& s=record->state();
      if(paused && !(s.requested&0x100U)) continue;
      if(filter && *filter!=handle) continue;
      if(s.script_reference) {
        if(!resolved_owner || !services.owner_update) throw std::runtime_error("Ordinary owner update route is unavailable");
        services.owner_update(*resolved_owner,*record);continue;
      }
      const auto captured_owner=s.attached_owner;
      if((s.requested&1U) && !(s.status&4U)) {
        if(!services.phase_one_diagnostic) throw std::runtime_error("Ordinary phase-one gate requires a diagnostic");
        services.phase_one_diagnostic(*record);continue;
      }
      if(!services.direct_event16 || (services.profiling && (!services.profile_begin || !services.profile_end)))
        throw std::runtime_error("Ordinary event16/profiling service missing");
      if(services.profiling) services.profile_begin(*record);
      services.direct_event16(*record);
      if(!s.attached_owner) continue;
      if(services.profiling) services.profile_end(*record);
      if(s.status&1U) {
        if(!services.retire) throw std::runtime_error("Ordinary retirement service missing");
        services.retire(*record,captured_owner);
      }
    }
    traversing_=false;
  } catch(...) {traversing_=false;failed_=true;throw;}
}
} // namespace off::runtime
