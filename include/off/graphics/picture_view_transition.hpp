#pragma once

#include "off/graphics/intro_camera_state.hpp"
#include "off/graphics/picture_material_state.hpp"

#include <functional>

namespace off::graphics {

struct PictureStoredViewRectangle {
  std::uint32_t left, top, right, bottom;
};
struct PictureDeviceViewport {
  std::uint32_t x, y, width, height;
  float minimum_depth, maximum_depth;
};
struct PictureViewClear {
  bool color, depth, stencil;
  std::uint32_t packed_color;
  float depth_value;
  std::uint32_t stencil_value;
  // No explicit rectangles: clear within the current device viewport.
};
struct PictureViewFogState {
  PictureRendererFogState colors;
  bool suppression_latched;
  bool tracked_enabled;
  float start, end;
};
struct PictureViewTransitionHooks {
  std::function<void(const PictureDeviceViewport&)> viewport;
  std::function<void(bool)> fog_enabled;
  std::function<void(std::uint32_t)> fog_color;
  std::function<void()> vertex_fog_linear;
  std::function<void()> table_fog_none;
  std::function<void(float)> fog_start;
  std::function<void(float)> fog_end;
  std::function<void(const PictureViewClear&)> clear;
};

// One instance per allocated live view, not per camera/key/pass. Allocation
// initializes the guard to zero, including allocation of a reused pool slot.
class PictureViewTransition final {
public:
  PictureViewTransition() = default;
  PictureViewTransition(const PictureViewTransition&) = delete;
  PictureViewTransition& operator=(const PictureViewTransition&) = delete;
  PictureViewTransition(PictureViewTransition&&) = delete;
  PictureViewTransition& operator=(PictureViewTransition&&) = delete;
  [[nodiscard]] std::uint32_t last_clear_frame() const noexcept { return last_clear_frame_; }

  // Caller resolves the already-associated camera from the live intermediate.
  // This is not admission or frustum preparation. All hooks and finite bounded
  // arithmetic are validated before effects (native policy); control 4 rejects.
  // Hooks must preserve referenced lifetimes/state and must not reenter. A
  // throwing backend retains its completed prefix: abort the frame, no rollback.
  void run(std::uint32_t frame, const IntroCameraState& camera,
           std::uint32_t camera_flags, std::uint32_t render_control,
           const PictureStoredViewRectangle& rectangle, float global_fog_control,
           bool stencil_available, std::uint32_t& draw_activity_frame,
           PictureViewFogState& fog, const PictureViewTransitionHooks& hooks);
private:
  std::uint32_t last_clear_frame_{0};
  bool running_{false};
};

} // namespace off::graphics
