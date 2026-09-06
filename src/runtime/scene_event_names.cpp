#include "off/runtime/scene_event_names.hpp"
#include <stdexcept>
#include <utility>

namespace off::runtime {
std::string SceneEventNames::canonicalize(std::string_view name) {
  if(name.find('\0')!=std::string_view::npos)
    throw std::runtime_error("Scene event name contains embedded NUL");
  std::string result(name);
  for(auto& value:result) if(value>='A' && value<='Z') value=static_cast<char>(value-'A'+'a');
  return result;
}
std::uint16_t SceneEventNames::insert(std::string canonical,std::uint16_t requested) {
  const auto found=by_name_.find(canonical);
  if(found!=by_name_.end()) return found->second;
  const auto identity=requested?static_cast<std::uint32_t>(requested):counter_+1U;
  if(identity==0 || identity>=capacity)
    throw std::runtime_error("Scene event identity capacity exhausted");
  if(by_identity_[identity])
    throw std::runtime_error("Scene event identity collides with a different name");
  // Prepare both owned name copies before committing either index. Allocation
  // failure may preserve completed lazy initialization, not half an entry.
  std::optional<std::string> reverse{canonical};
  const auto narrowed=static_cast<std::uint16_t>(identity);
  by_name_.emplace(std::move(canonical),narrowed);
  by_identity_[identity]=std::move(reverse);
  if(!requested) counter_=identity;
  return narrowed;
}
std::uint16_t SceneEventNames::declare(std::string_view name,std::uint16_t requested) {
  auto canonical=canonicalize(name);
  if(!initialized_) {
    // Retrying after an allocation failure reuses any completed reserved entry.
    insert("cam_entercamera",0x401);
    insert("cam_leavecamera",0x400);
    insert("cam_leavecamera",0x400);
    initialized_=true;
  }
  return insert(std::move(canonical),requested);
}
std::optional<std::uint16_t> SceneEventNames::find(std::string_view name) const {
  const auto found=by_name_.find(canonicalize(name));
  return found==by_name_.end()?std::nullopt:std::optional{found->second};
}
std::optional<std::string_view> SceneEventNames::name(std::uint16_t identity) const {
  if(identity>=capacity) throw std::runtime_error("Scene event identity is out of range");
  if(!by_identity_[identity]) return std::nullopt;
  return *by_identity_[identity];
}
void SceneEventNames::clear() noexcept {
  by_name_.clear();
  for(auto& name:by_identity_) name.reset();
  counter_=0;
  initialized_=false;
}
} // namespace off::runtime
