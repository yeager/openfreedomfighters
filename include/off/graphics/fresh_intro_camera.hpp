#pragma once

#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/intro_picture_transform.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {

// Constructed normally without a source, or from a supported fresh
// mode-zero/selector-zero authored source.
// Owns the reviewed CPU fields, not a complete engine owner or registered view.
// In particular, the separate scene-root owner pointer is not represented here.
class FreshIntroCamera final {
public:
  FreshIntroCamera();
  explicit FreshIntroCamera(const data::GmsIntroCameraSource &source);
  FreshIntroCamera(const FreshIntroCamera &) = delete;
  FreshIntroCamera &operator=(const FreshIntroCamera &) = delete;
  FreshIntroCamera(FreshIntroCamera &&) = delete;
  FreshIntroCamera &operator=(FreshIntroCamera &&) = delete;

  [[nodiscard]] const IntroCameraState &parameters() const noexcept { return parameters_; }
  [[nodiscard]] std::uint32_t flags() const noexcept { return enabled_state_.flags(); }
  [[nodiscard]] bool enabled() const noexcept { return enabled_state_.enabled(); }
  [[nodiscard]] std::uint32_t render_control() const noexcept { return render_control_; }
  [[nodiscard]] std::int32_t priority() const noexcept { return priority_; }
  // Camera priority is independent of the renderer registry's insertion key.
  void set_priority(std::int32_t priority) noexcept;
  // Preview Y-key direct camera-owner flag mutation, with no renderer or
  // resource/transform side effects. This is not an enabled-state transition.
  void toggle_preview_flag();
  // Loader's direct OR, distinct from the held-key XOR operation.
  void enable_preview_flag();
  [[nodiscard]] float renderer_width() const noexcept { return renderer_width_; }
  [[nodiscard]] float renderer_height() const noexcept { return renderer_height_; }
  // Query/store width, then query/store height. A later exception preserves the
  // completed prefix; no projection, viewport, flags or context are changed.
  void notify_renderer_dimensions(const std::function<std::int32_t()>& width,
                                  const std::function<std::int32_t()>& height);
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
  std::int32_t priority_{};
  float renderer_width_{},renderer_height_{};
  bool notifying_dimensions_{};
  std::optional<std::uint64_t> associated_target_;
  std::optional<PictureCameraServices> picture_services_;
};

} // namespace off::graphics
