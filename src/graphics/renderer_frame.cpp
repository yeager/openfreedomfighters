#include "off/graphics/renderer_frame.hpp"

#include <stdexcept>

namespace off::graphics {

RendererFrameOutcome RendererFrame::run(RendererFrameClock& clock,
    bool engine_running, bool renderer_initialized,
    const RendererFrameLifecycleHooks& hooks) {
  if (running_ || clock.coordinating_)
    throw std::runtime_error("renderer frame coordination cannot reenter");
  if (!engine_running || !renderer_initialized) return RendererFrameOutcome::skipped;
  if (!hooks.admit_device_scene || !hooks.backend_traversal || !hooks.admitted_post_render ||
      !hooks.end_scene || !hooks.renderer_completion)
    throw std::runtime_error("renderer frame lifecycle requires complete hooks");
  struct Guard {
    bool& renderer;
    bool& engine;
    Guard(bool& renderer_active, bool& engine_active) : renderer(renderer_active), engine(engine_active) {
      renderer = true; engine = true;
    }
    ~Guard() { engine = false; renderer = false; }
  } guard(running_, clock.coordinating_);

  const bool admitted = hooks.admit_device_scene();
  if (admitted) {
    hooks.backend_traversal();
    hooks.admitted_post_render();
    hooks.end_scene();
  }
  hooks.renderer_completion();
  clock.frame_ += std::uint32_t{1};
  return admitted ? RendererFrameOutcome::rendered : RendererFrameOutcome::admission_failed;
}

} // namespace off::graphics
