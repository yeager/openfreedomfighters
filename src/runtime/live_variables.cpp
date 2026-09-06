#include "off/runtime/live_variables.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace off::runtime {
LiveVariableLease::~LiveVariableLease() {reset();}
LiveVariableLease::LiveVariableLease(LiveVariableLease&& other) noexcept
  :registry_(std::exchange(other.registry_,nullptr)),handle_(std::exchange(other.handle_,{})) {}
LiveVariableLease& LiveVariableLease::operator=(LiveVariableLease&& other) noexcept {
  if(this!=&other) {
    reset();registry_=std::exchange(other.registry_,nullptr);handle_=std::exchange(other.handle_,{});
  }
  return *this;
}
void LiveVariableLease::reset() noexcept {
  if(registry_) registry_->release(handle_);
  registry_=nullptr;handle_={};
}
LiveVariableLease LiveVariableRegistry::insert(std::string name,std::variant<bool*,float*> storage) {
  if(name.empty() || next_==0) throw std::runtime_error("Live variable name or identity domain is unavailable");
  const LiveVariableHandle handle{next_,this};
  entries_.emplace(handle.identity,Entry{std::move(name),storage});
  ++next_; // Zero after the final identity permanently marks exhaustion.
  return LiveVariableLease(*this,handle);
}
LiveVariableLease LiveVariableRegistry::bind(std::string name,bool& storage) {
  return insert(std::move(name),&storage);
}
LiveVariableLease LiveVariableRegistry::bind(std::string name,float& storage) {
  if(!std::isfinite(storage)) throw std::runtime_error("Live variable float must be finite");
  return insert(std::move(name),&storage);
}
void LiveVariableRegistry::release(LiveVariableHandle handle) noexcept {
  if(handle.registry==this) entries_.erase(handle.identity);
}
bool LiveVariableRegistry::contains(LiveVariableHandle handle) const noexcept {
  return handle.registry==this && entries_.contains(handle.identity);
}
const LiveVariableRegistry::Entry& LiveVariableRegistry::lookup(LiveVariableHandle handle) const {
  const auto entry=entries_.find(handle.identity);
  if(handle.registry!=this || entry==entries_.end()) throw std::runtime_error("Live variable handle is stale or invalid");
  return entry->second;
}
std::vector<LiveVariableHandle> LiveVariableRegistry::enumerate(std::string_view name) const {
  std::vector<LiveVariableHandle> result;
  for(const auto& [id,entry]:entries_) if(entry.name==name) result.push_back({id,this});
  return result;
}
LiveVariableType LiveVariableRegistry::type(LiveVariableHandle handle) const {
  return std::holds_alternative<bool*>(lookup(handle).storage)?LiveVariableType::boolean:LiveVariableType::floating;
}
bool LiveVariableRegistry::read_bool(LiveVariableHandle handle) const {
  const auto* storage=std::get_if<bool*>(&lookup(handle).storage);
  if(!storage) throw std::runtime_error("Live variable is not Boolean");
  return **storage;
}
float LiveVariableRegistry::read_float(LiveVariableHandle handle) const {
  const auto* storage=std::get_if<float*>(&lookup(handle).storage);
  if(!storage) throw std::runtime_error("Live variable is not floating point");
  if(!std::isfinite(**storage)) throw std::runtime_error("Live variable float must be finite");
  return **storage;
}
void LiveVariableRegistry::write_bool(LiveVariableHandle handle,bool value) {
  const auto* storage=std::get_if<bool*>(&lookup(handle).storage);
  if(!storage) throw std::runtime_error("Live variable is not Boolean");
  **storage=value;
}
void LiveVariableRegistry::write_float(LiveVariableHandle handle,float value) {
  const auto* storage=std::get_if<float*>(&lookup(handle).storage);
  if(!storage) throw std::runtime_error("Live variable is not floating point");
  if(!std::isfinite(value)) throw std::runtime_error("Live variable float must be finite");
  **storage=value;
}
} // namespace off::runtime
