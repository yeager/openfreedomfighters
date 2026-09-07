#include "off/runtime/component_lifecycle.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::runtime {
namespace {
std::string describe(const ComponentRecord& record) {
  const auto& source=record.source();
  return source.factory_name + " owner=" + std::to_string(source.owner) +
      " source=" + (source.directory_index ? std::to_string(*source.directory_index) : "synthesized") +
      " attachment=" + (source.attachment_index ? std::to_string(*source.attachment_index) : "none");
}
}
SceneComponentSequence::SceneComponentSequence(std::function<std::uint32_t()> dispatch_clock)
    : dispatch_clock_(std::move(dispatch_clock)) {
  if(!dispatch_clock_) throw std::runtime_error("Live component dispatch clock supplier is required");
}
float ComponentRecord::scheduling_interval() const {
  if(!schedule_) throw std::runtime_error("Component schedule is not constructed");
  return schedule_->interval;
}
std::uint32_t ComponentRecord::scheduling_clock() const {
  if(!schedule_) throw std::runtime_error("Component schedule is not constructed");
  return schedule_->clock;
}
ComponentLifecycle::~ComponentLifecycle() {
  // Native owner teardown, not an emulation of original per-class destruction.
  for (const auto& record : records_)
    if (record->identity_ && !record->removed_) --sequence_.live_;
}
ComponentState& ComponentRecord::state() {
  if (!instance_) throw std::runtime_error("Component is not constructed: " + source_.factory_name);
  return instance_->state;
}
const ComponentState& ComponentRecord::state() const {
  if (!instance_) throw std::runtime_error("Component is not constructed: " + source_.factory_name);
  return instance_->state;
}
void ComponentLifecycle::check_idle() const {
  if (failed_) throw std::runtime_error("Component lifecycle previously failed");
  if (busy_ || sequence_.busy_)
    throw std::runtime_error("Scene component lifecycle mutation/reentry is unsupported");
}
std::size_t ComponentLifecycle::append(ComponentSource source) {
  check_idle();
  if (source.owner == 0 || source.factory_name.empty() || !std::isfinite(source.authored_parameter))
    throw std::runtime_error("Invalid retained component source");
  for (const auto& record : records_) {
    const auto& old = record->source();
    if (old.owner == source.owner && old.attachment_index == source.attachment_index &&
        old.synthesized == source.synthesized)
      throw std::runtime_error("Duplicate retained component source");
  }
  records_.push_back(std::unique_ptr<ComponentRecord>(new ComponentRecord(std::move(source))));
  completed_ = false;
  return records_.size() - 1;
}
void ComponentLifecycle::construct(std::size_t index, const Factory& supplied) {
  check_idle();
  auto& record = at(index);
  if (record.identity_ || !supplied) throw std::runtime_error("Missing factory or repeated component construction");
  if (sequence_.next_ > std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("Scene component identity exhausted");
  const auto factory = supplied;
  order_.reserve(order_.size() + 1);
  busy_ = true;
  sequence_.busy_ = true;
  try {
    construct_common(index);
    constructing_=index;
    record.instance_ = factory(record);
    if(failed_) throw std::runtime_error("Nested component construction previously failed");
    record.constructed_ = true;
    constructing_.reset();
    busy_ = false;
    sequence_.busy_ = false;
  } catch (...) { failed_ = true; constructing_.reset(); busy_ = false; sequence_.busy_ = false; throw; }
}
void ComponentLifecycle::construct_common(std::size_t index) {
  auto& record=at(index);
  const auto dispatch_time=sequence_.dispatch_clock_();
  // Force each approved binary32 rounding boundary independently of contraction.
  const volatile float scaled_phase=sequence_.phase_*0.1F;
  const volatile float offset=scaled_phase*-1024.0F;
  const volatile float advanced=sequence_.phase_+0.1F;
  record.schedule_=ComponentRecord::Schedule{0.1F,
      dispatch_time-static_cast<std::uint32_t>(static_cast<std::int32_t>(offset))};
  sequence_.phase_=advanced>=1.0F?0.0F:advanced;
  record.identity_ = static_cast<std::uint32_t>(sequence_.next_++);
  ++sequence_.live_;
  record.previous_=tail_;
  record.next_.reset();
  if(tail_) at(*tail_).next_=index;
  tail_=index;
  order_.push_back(index);
  record.instance_ = ConstructedComponent{
      {0,0,0,0,0,sequence_.construction_mode_ ? 0x10U : 0U,0,0},{},{}};
  completed_ = false;
}
void ComponentLifecycle::set_optional_lookup_removal(std::function<void(std::uint32_t)> removal) {
  check_idle();
  optional_lookup_removal_=std::move(removal);
}
void ComponentLifecycle::construct_and_destroy_temporary_common() {
  if(failed_ || !busy_ || !sequence_.busy_ || !constructing_ || temporary_active_)
    throw std::runtime_error("Temporary common construction requires its active concrete factory");
  if(sequence_.next_>std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("Scene component identity exhausted");
  const auto index=records_.size();
  records_.reserve(index+1);
  order_.reserve(order_.size()+1);
  // Native transient metadata: no authored attachment, owner or class event.
  auto temporary=std::unique_ptr<ComponentRecord>(new ComponentRecord(
      {0,std::nullopt,std::nullopt,"TemporaryCommon",0,0,0.0F,true}));
  records_.push_back(std::move(temporary));
  temporary_active_=true;
  try {
    construct_common(index);
    auto& record=at(index);
    record.state().status|=0x20U;
    record.constructed_=true;
    // Unlink the actual newest node before the optional lookup callback. No
    // callback may mutate construction order while this nested route runs.
    unlink_live_node(index);
    order_.pop_back();
    if(!(record.state().status&0x10U) && optional_lookup_removal_)
      optional_lookup_removal_(*record.identity_);
    --sequence_.live_;
    record.removed_=true;
    records_.pop_back();
    temporary_active_=false;
  } catch(...) {failed_=true;temporary_active_=false;throw;}
}
void ComponentLifecycle::unlink_live_node(std::size_t index) {
  auto& record=at(index);
  if(record.removed_) throw std::runtime_error("Component is already unlinked");
  const auto previous=record.previous_;
  const auto next=record.next_;
  if(next) at(*next).previous_=previous;
  else tail_=previous;
  if(previous) at(*previous).next_=next;
  record.previous_.reset();
  record.next_.reset();
}
void ComponentLifecycle::pass(bool second, const ComponentLifecycleServices& services, std::size_t& visited) {
  // The second invocation enters here after phase one has returned and samples
  // tail_ again.  Save the live predecessor before every callback: concrete
  // callbacks may change fields but native policy rejects registry mutation.
  for (auto cursor=tail_;cursor;) {
    const auto current=*cursor;
    auto& record=at(current);
    const auto previous=record.previous_;
    ++visited;
    auto& state = record.state();
    const auto mask = second ? 2U : 1U;
    if (state.requested & mask) {
      services.progress(second,record,visited);
      if (state.attached_owner != 0) {
        const auto owner = state.attached_owner;
        const bool bypass=(state.requested & 0x200U)!=0;
        const auto flags = services.owner_flags(owner);
        if(flags) {
          if (bypass || !(*flags & 0x400U)) {
            if (!(state.status & 1U)) {
              const auto callback = second ? record.instance_->phase_two : record.instance_->phase_one;
              if (!callback)
                throw std::runtime_error("Unsupported component " + describe(record) +
                                         (second ? " phase two" : " phase one"));
              callback(record);
              state.status |= second ? 8U : 4U;
            }
            if (state.status & 1U) {
              services.retire(record);
              unlink_live_node(current);
              record.removed_ = true;
              --sequence_.live_;
              record.instance_->phase_one = {};
              record.instance_->phase_two = {};
            }
          }
          if (!second) services.post_phase_one(owner);
        }
      }
    }
    cursor=previous;
  }
}
void ComponentLifecycle::validate_global_phase_entry(const ComponentLifecycleServices& services) const {
  if (!services.progress || !services.owner_flags || !services.post_phase_one || !services.retire)
    throw std::runtime_error("Incomplete global component lifecycle services");
  for (const auto& record : records_)
    if (!record->constructed())
      throw std::runtime_error("Unconstructed component: " + describe(*record));
  std::size_t local_live=0;
  for (const auto& record : records_) if (record->identity_ && !record->removed_) ++local_live;
  if (sequence_.live_ != local_live)
    throw std::runtime_error("Global phases require all live scene components in the same registry");
  if (order_.size() > std::numeric_limits<std::size_t>::max()/2)
    throw std::runtime_error("Global component progress counter would overflow");
}
void ComponentLifecycle::run_global_phases_locked(const ComponentLifecycleServices& services) {
  std::size_t visited=0;
  pass(false, services,visited);
  pass(true, services,visited);
}
void ComponentLifecycle::run_global_phases(const ComponentLifecycleServices& supplied) {
  check_idle();
  const auto services = supplied;
  validate_global_phase_entry(services);
  busy_ = true;
  sequence_.busy_ = true;
  completed_ = false;
  try {
    run_global_phases_locked(services);
    completed_ = true;
    busy_ = false;
    sequence_.busy_ = false;
  } catch (...) { failed_ = true; busy_ = false; sequence_.busy_ = false; throw; }
}
void ComponentLifecycle::run_global_lifecycle(
    std::span<const std::uint64_t> additional_resources,
    const GlobalComponentLifecycleServices& supplied) {
  check_idle();
  const auto services=supplied;
  validate_global_phase_entry(services.components);
  if(!services.capture_progress_denominator || !services.root_pre_global || !services.root_post_global ||
      !services.additional_owner || !services.additional_pre_global || !services.additional_post_global ||
      !services.mark_resource_initialized)
    throw std::runtime_error("Incomplete outer global lifecycle services");
  if(additional_resources.size()>(std::numeric_limits<std::size_t>::max()-static_cast<std::size_t>(sequence_.live_))/2U)
    throw std::runtime_error("Global lifecycle progress denominator would overflow");

  // Preflight rejects null resource handles before any external hook observes a
  // partial scene.  It intentionally cannot snapshot live owners: the
  // reviewed route resolves them again at every visit boundary.
  for(const auto resource:additional_resources)
    if(resource==0) throw std::runtime_error("Additional global lifecycle resource is null");
  const auto denominator=additional_resources.size()*2U+static_cast<std::size_t>(sequence_.live_);
  busy_=true;
  sequence_.busy_=true;
  completed_=false;
  try {
    services.capture_progress_denominator(denominator);
    services.root_pre_global();
    for(const auto resource:additional_resources) {
      const auto owner=services.additional_owner(resource);
      if(!owner) throw std::runtime_error("Additional global lifecycle resource is unresolved");
      if(*owner!=0) services.additional_pre_global(*owner);
    }

    run_global_phases_locked(services.components);

    services.root_post_global();
    for(const auto resource:additional_resources) {
      const auto owner=services.additional_owner(resource);
      if(!owner) throw std::runtime_error("Additional global lifecycle resource is unresolved");
      services.mark_resource_initialized(resource);
      if(*owner!=0) services.additional_post_global(*owner);
    }
    completed_=true;
    busy_=false;
    sequence_.busy_=false;
  } catch(...) {
    failed_=true;
    busy_=false;
    sequence_.busy_=false;
    throw;
  }
}
} // namespace off::runtime
