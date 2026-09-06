#pragma once
#include "off/runtime/live_variables.hpp"
#include "off/graphics/preview_camera_update.hpp"
#include <array>

namespace off::runtime { class ApplicationServices; }
namespace off::graphics {
// Concrete PreviewCamera-owned payload. Ordinary component construction,
// binding, status/mask admission and callback dispatch belong to the lifecycle.
// Stable address is required by live bindings. Registry must outlive component.
class PreviewCameraComponent final {
public:
  explicit PreviewCameraComponent(runtime::LiveVariableRegistry& registry);
  ~PreviewCameraComponent();
  PreviewCameraComponent(const PreviewCameraComponent&) = delete;
  PreviewCameraComponent& operator=(const PreviewCameraComponent&) = delete;
  [[nodiscard]] bool collision_enabled() const noexcept {return collision_enabled_;}
  [[nodiscard]] float collision_length() const noexcept {return collision_length_;}
  [[nodiscard]] bool dynamic_check() const noexcept {return dynamic_check_;}
  [[nodiscard]] bool static_check() const noexcept {return static_check_;}
  [[nodiscard]] std::array<runtime::LiveVariableHandle,4> handles() const noexcept;
  // Called only through admitted ordinary dispatch. Uses this component's live
  // collision variable and the application's canonical clock/pointer history.
  // Keyboard/debug-collision branches remain explicit implementation boundaries.
  void update(runtime::ApplicationServices& application,PreviewCameraPose& camera,
      PreviewCameraInput input,const std::function<void(PreviewCameraPose&)>& enqueue_transform);
private:
  bool collision_enabled_{};
  float collision_length_{1000.0F};
  bool dynamic_check_{true},static_check_{true};
  std::array<runtime::LiveVariableLease,4> variables_;
};
} // namespace off::graphics
