#include "off/runtime/input_maps.hpp"
#include <limits>
#include <stdexcept>
#include <utility>
namespace off::runtime {
InputMapHandle InputMapRegistry::acquire(InputMapIdentity identity,std::string name,std::uint32_t option) {
  if(identity!=InputMapIdentity::root_control || name!="RootControl" || option!=0)
    throw std::runtime_error("Unsupported input map registration");
  for(std::size_t i=0;i<records_.size();++i) if(records_[i].identity==identity) {
    if(records_[i].references==std::numeric_limits<std::uint64_t>::max())
      throw std::runtime_error("Input map reference count exhausted");
    ++records_[i].references;
    return {i+1,this};
  }
  records_.push_back({identity,std::move(name),option,1});
  return {records_.size(),this};
}
const InputMapRecord& InputMapRegistry::at(InputMapHandle handle) const {
  if(handle.registry!=this || !handle.value || handle.value>records_.size())
    throw std::runtime_error("Invalid input map handle");
  return records_[static_cast<std::size_t>(handle.value-1)];
}
}
