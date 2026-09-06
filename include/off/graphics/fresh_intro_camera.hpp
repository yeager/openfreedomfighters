#pragma once

#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/intro_picture_transform.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {

// Constructed from the supported fresh mode-zero/selector-zero camera source.
// Owns the reviewed CPU fields, not a complete engine owner or registered view.
// In particular, the separate scene-root owner pointer is not represented here.
class FreshIntroCamera final {
public:
  explicit FreshIntroCamera(const data::GmsIntroCameraSource &source);
  FreshIntroCamera(const FreshIntroCamera &) = delete;
  FreshIntroCamera &operator=(const FreshIntroCamera &) = delete;
  FreshIntroCamera(FreshIntroCamera &&) = delete;
  FreshIntroCamera &operator=(FreshIntroCamera &&) = delete;

  [[nodiscard]] const IntroCameraState &parameters() const noexcept { return parameters_; }
  [[nodiscard]] std::uint32_t flags() const noexcept { return enabled_state_.flags(); }
  [[nodiscard]] bool enabled() const noexcept { return enabled_state_.enabled(); }
  [[nodiscard]] std::uint32_t render_control() const noexcept { return render_control_; }
  // A future native runtime identity, never an authored source reference.
  [[nodiscard]] std::optional<std::uint64_t> associated_target() const noexcept { return associated_target_; }

  // Same canonical flags as flags()/enabled(); no detached renderer-state copy.
  // The caller establishes renderer presence and stable lifetime. Notification
  // precedes the changed enabled bit; this operation does not register a camera.
  void set_enabled(bool requested, bool renderer_present,
                   const std::function<void()> &state_change);

  // Retains the picture-service projection in this camera, updating its one
  // canonical flag word. No registration or complete frustum preparation.
  // Failure preserves the previous snapshot; the caller must stop the failed
  // frame rather than submit using that stale snapshot.
  void prepare_picture_services(const PictureVisitorRectangle& rectangle);
  // Explicit camera-only projection of an admitted selected-window initializer.
  // Input/scheduler work and renderer registration remain caller responsibilities.
  void apply_window_state_projection(bool option_a, bool option_b,
                                     std::uint64_t window_handle);
  [[nodiscard]] const std::optional<PictureCameraServices>& picture_services() const noexcept {
    return picture_services_;
  }

private:
  IntroCameraState parameters_;
  CameraEnabledState enabled_state_;
  std::uint32_t render_control_{0};
  std::optional<std::uint64_t> associated_target_;
  std::optional<PictureCameraServices> picture_services_;
};

} // namespace off::graphics
