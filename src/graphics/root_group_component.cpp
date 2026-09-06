#include "off/graphics/root_group_component.hpp"
#include <stdexcept>
namespace off::graphics {
RootGroupComponent::RootGroupComponent(runtime::LiveVariableRegistry* console,runtime::InputMapRegistry* input_maps)
    :input_maps_(input_maps) {
  if(console) display_lease_=console->bind("DisplayName",display_name_);
}
void RootGroupComponent::bind_owner(RootGroupOwnerHandle owner) {
  if(!owner.value || owner_.value) throw std::runtime_error("RootGroup requires fresh nonnull owner binding");
  owner_=owner;
}
void RootGroupComponent::initialize() {
  initialized_=false;
  display_name_=0.0F;
  auxiliary_=0;
  latch_=false;
  if(input_maps_) map_=input_maps_->acquire(runtime::InputMapIdentity::root_control,"RootControl",0);
  initialized_=true;
}
}
