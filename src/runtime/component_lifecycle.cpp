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
  record.identity_ = static_cast<std::uint32_t>(sequence_.next_++);
  ++sequence_.live_;
  order_.push_back(index);
  record.instance_ = ConstructedComponent{
      {0,0,0,0,0,sequence_.construction_mode_ ? 0x10U : 0U,0,0},{},{}};
  completed_ = false;
  busy_ = true;
  sequence_.busy_ = true;
  try {
    record.instance_ = factory(record);
    record.constructed_ = true;
    busy_ = false;
    sequence_.busy_ = false;
  } catch (...) { failed_ = true; busy_ = false; sequence_.busy_ = false; throw; }
}
void ComponentLifecycle::pass(bool second, const ComponentLifecycleServices& services, std::size_t& visited) {
  // Stable native boundary: callbacks may change live fields, not this list.
  for (auto cursor = order_.size(); cursor > 0;) {
    auto& record = at(order_[--cursor]);
    if (record.removed_) continue;
    ++visited;
    auto& state = record.state();
    const auto mask = second ? 2U : 1U;
    if (!(state.requested & mask)) continue;
    services.progress(second,record,visited);
    if (state.attached_owner == 0) continue;
    const auto owner = state.attached_owner;
    const bool bypass=(state.requested & 0x200U)!=0;
    const auto flags = services.owner_flags(owner);
    if (!flags) continue;
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
        record.removed_ = true;
        --sequence_.live_;
        record.instance_->phase_one = {};
        record.instance_->phase_two = {};
      }
    }
    if (!second) services.post_phase_one(owner);
  }
}
void ComponentLifecycle::run_global_phases(const ComponentLifecycleServices& supplied) {
  check_idle();
  const auto services = supplied;
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
  busy_ = true;
  sequence_.busy_ = true;
  completed_ = false;
  try {
    std::size_t visited=0;
    pass(false, services,visited);
    pass(true, services,visited);
    completed_ = true;
    busy_ = false;
    sequence_.busy_ = false;
  } catch (...) { failed_ = true; busy_ = false; sequence_.busy_ = false; throw; }
}
} // namespace off::runtime
