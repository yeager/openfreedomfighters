#include "off/graphics/picture_view_transition.hpp"

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
float finite(float value) {
  if (!std::isfinite(value)) throw std::runtime_error("view transition requires finite arithmetic");
  return value;
}
float multiply(float a, float b) {
  const volatile float value = a * b;
  return finite(value);
}
float add(float a, float b) {
  const volatile float value = a + b;
  return finite(value);
}
float unsigned_float(std::uint32_t value) {
  const volatile float result = static_cast<float>(value);
  return result;
}
std::uint32_t device_integer(float value, bool extent) {
  if (!std::isfinite(value) || value < 0 ||
      static_cast<double>(value) > std::numeric_limits<std::uint32_t>::max() ||
      (extent && value < 1))
    throw std::runtime_error("view transition viewport is outside the supported integer range");
  return static_cast<std::uint32_t>(value); // Each component truncates separately.
}
PictureDeviceViewport viewport(const PictureStoredViewRectangle& r, const std::array<float, 4>& v) {
  if (r.right <= r.left || r.bottom <= r.top)
    throw std::runtime_error("view transition requires a nonwrapping positive rectangle");
  for (const auto component : v) static_cast<void>(finite(component));
  const auto width = unsigned_float(r.right - r.left);
  const auto height = unsigned_float(r.bottom - r.top);
  return {
      device_integer(add(unsigned_float(r.left), multiply(width, std::max(v[0], 0.0F))), false),
      device_integer(add(unsigned_float(r.top), multiply(height, std::max(v[1], 0.0F))), false),
      device_integer(multiply(width, std::min(v[2], 1.0F)), true),
      device_integer(multiply(height, std::min(v[3], 1.0F)), true), 0, 1};
}
}

void PictureViewTransition::run(std::uint32_t frame, const IntroCameraState& camera,
    std::uint32_t camera_flags, std::uint32_t render_control,
    const PictureStoredViewRectangle& rectangle, float global_fog_control,
    bool stencil_available, std::uint32_t& draw_activity_frame,
    PictureViewFogState& fog, const PictureViewTransitionHooks& hooks) {
  if (running_ || render_control == 4 || std::fegetround() != FE_TONEAREST)
    throw std::runtime_error("unsupported or reentrant view transition");
  if (!hooks.viewport || !hooks.fog_enabled || !hooks.fog_color ||
      !hooks.vertex_fog_linear || !hooks.table_fog_none || !hooks.fog_start ||
      !hooks.fog_end || !hooks.clear)
    throw std::runtime_error("view transition requires complete backend hooks");
  const auto request = viewport(rectangle, camera.viewport);
  static_cast<void>(finite(global_fog_control));
  const auto start = multiply(finite(camera.far_distance), finite(camera.fog_start_fraction));
  auto end = multiply(camera.far_distance, finite(camera.fog_end_fraction));
  if (end == 0) end = camera.far_distance;
  const auto background = camera.background == 1 ? 0 : camera.background;
  struct Guard {
    bool& running;
    explicit Guard(bool& value) : running(value) { running = true; }
    ~Guard() { running = false; }
  } guard(running_);
  draw_activity_frame = frame;
  hooks.viewport(request);
  hooks.viewport(request);
  if (global_fog_control == 0 || (camera_flags & 0x80000U) != 0) {
    if (!fog.suppression_latched) {
      const bool was_enabled = fog.tracked_enabled;
      fog.tracked_enabled = false;
      if (was_enabled) hooks.fog_enabled(false);
    }
    fog.suppression_latched = true;
  }
  fog.start = start;
  fog.end = end;
  fog.colors.base_color = background | 0xff000000U;
  hooks.fog_color(fog.colors.base_color);
  hooks.vertex_fog_linear();
  hooks.table_fog_none();
  hooks.fog_start(start);
  hooks.fog_end(end);
  // Configuration deliberately leaves colors.tracked_color untouched.
  if (last_clear_frame_ == frame) return;
  last_clear_frame_ = frame;
  if ((camera_flags & 0x8000U) != 0) return;
  hooks.clear({render_control != 5, true, stencil_available,
               render_control == 0 ? background : 0U, 1, 0});
}
} // namespace off::graphics
