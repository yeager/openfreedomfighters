#include "off/graphics/preview_camera_component.hpp"
#include "off/runtime/application_services.hpp"

namespace off::graphics {
PreviewCameraComponent::PreviewCameraComponent(runtime::LiveVariableRegistry& registry) {
  try {
    variables_[0]=registry.bind("cam_coli_check_dynamic",dynamic_check_);
    // The static variable intentionally aliases the dynamic field. Static state
    // is separate and remains true; this is the approved observed binding.
    variables_[1]=registry.bind("cam_coli_check_static",dynamic_check_);
    variables_[2]=registry.bind("cam_coli_enable",collision_enabled_);
    variables_[3]=registry.bind("cam_coli_len",collision_length_);
  } catch(...) {
    for(auto& variable:variables_) variable.reset();
    throw;
  }
}
PreviewCameraComponent::~PreviewCameraComponent() {
  // Release in registration order, before component storage disappears.
  for(auto& variable:variables_) variable.reset();
}
std::array<runtime::LiveVariableHandle,4> PreviewCameraComponent::handles() const noexcept {
  return {variables_[0].handle(),variables_[1].handle(),variables_[2].handle(),variables_[3].handle()};
}
void PreviewCameraComponent::update(runtime::ApplicationServices& application,
    FreshIntroCamera& owner,PreviewCameraPose& camera,PreviewCameraInput input,
    const std::function<void(PreviewCameraPose&)>& enqueue_transform) {
  if(variables_[0].handle().registry!=&application.live_variables())
    throw std::runtime_error("Preview camera variables belong to a different application");
  input.collision_visualization=collision_enabled_;
  application.update_preview_camera(owner,camera,input,enqueue_transform);
}
} // namespace off::graphics
