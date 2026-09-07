#include "off/graphics/renderer_frame.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

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

void OrdinarySceneUpdate::run(bool render, LiveRendererNode* renderer_head,
                              const OrdinarySceneUpdateHooks& hooks) {
  if (running_ || !hooks.scheduled_events || !hooks.ordinary_components ||
      !hooks.position_and_bounds || !hooks.queued_maintenance ||
      (render && (!hooks.renderer_frame || !hooks.post_render))) {
    throw std::runtime_error("ordinary scene update requires complete services");
  }
  struct Guard {
    bool& value;
    explicit Guard(bool& current) : value(current) { value = true; }
    ~Guard() { value = false; }
  } guard(running_);
  hooks.scheduled_events();
  hooks.ordinary_components();
  hooks.position_and_bounds();
  hooks.queued_maintenance();
  if (!render) return;

  // A finite bound rejects accidental cycles in the bounded host.  Importantly,
  // it does not snapshot `next`: the link is read only after the callback.
  std::size_t visited = 0;
  for (auto* current = renderer_head; current != nullptr;) {
    if (visited++ == 4096U)
      throw std::runtime_error("renderer chain exceeds bounded scene traversal");
    hooks.renderer_frame(*current);
    current = current->next;
  }
  hooks.post_render();
}

RendererFrameOutcome EligibleRendererFrame::run(
    RendererFrameClock& clock, EligibleRendererFrameState& state,
    bool engine_running, bool renderer_initialized,
    const EligibleRendererFrameHooks& hooks) {
  if (running_ || clock.coordinating_)
    throw std::runtime_error("eligible renderer frame cannot reenter");
  if (!engine_running || !renderer_initialized)
    return RendererFrameOutcome::skipped;
  if (!hooks.device_suppressed || !hooks.backend_ready || !hooks.begin_scene ||
      !hooks.backend_traversal || !hooks.admitted_post_render || !hooks.end_scene ||
      !hooks.renderer_completion)
    throw std::runtime_error("eligible renderer frame requires complete services");
  struct Guard {
    bool& renderer;
    bool& engine;
    Guard(bool& renderer_active, bool& engine_active)
        : renderer(renderer_active), engine(engine_active) {
      renderer = true;
      engine = true;
    }
    ~Guard() { engine = false; renderer = false; }
  } guard(running_, clock.coordinating_);

  state.local_counter_a = 0;
  state.local_counter_b = 0;
  state.in_scene = false;
  const bool admitted = !hooks.device_suppressed() && hooks.backend_ready() &&
                        hooks.begin_scene();
  if (admitted) {
    state.in_scene = true;
    hooks.backend_traversal();
    hooks.admitted_post_render();
    hooks.end_scene();
    state.in_scene = false;
  }
  hooks.renderer_completion();
  clock.frame_ += std::uint32_t{1};
  return admitted ? RendererFrameOutcome::rendered
                  : RendererFrameOutcome::admission_failed;
}

void BackendTraversal::run(std::uint64_t renderer_identity,
                           std::span<const BackendStateInput> states,
                           const BackendTraversalHooks& hooks) {
  if (running_ || !hooks.state_frame_begin || !hooks.camera_enabled ||
      !hooks.begin_view_transform || !hooks.begin_view || !hooks.common_view ||
      !hooks.end_view || !hooks.state_maintenance || !hooks.backend_prepare ||
      !hooks.preselect || !hooks.draw_round || !hooks.restore_selected) {
    throw std::runtime_error("backend traversal requires complete services");
  }
  std::vector<BackendStateInput> retained;
  retained.reserve(states.size());
  for (const auto& state : states) {
    if (state.renderer_identity != renderer_identity) continue;
    if (state.views.size() > 16U)
      throw std::runtime_error("backend state exceeds admitted view capacity");
    retained.push_back(state);
  }
  struct Guard {
    bool& value;
    explicit Guard(bool& current) : value(current) { value = true; }
    ~Guard() { value = false; }
  } guard(running_);

  for (const auto& state : retained) {
    hooks.state_frame_begin(state.state_identity);
    // The policy requires stable, immutable view storage during this pass.  Do
    // not sort, copy, or re-resolve the cameras: consume array positions live.
    for (std::size_t index = 0; index < state.views.size(); ++index) {
      const auto& view = state.views[index];
      if (!view.camera_identity || !hooks.camera_enabled(*view.camera_identity)) continue;
      hooks.begin_view_transform(*view.camera_identity, view.identity);
      hooks.begin_view(state.state_identity, view.identity);
      hooks.common_view(state.state_identity, view.identity);
      hooks.end_view(state.state_identity, view.identity);
    }
  }
  for (const auto& state : retained) hooks.state_maintenance(state.state_identity);
  hooks.backend_prepare();

  std::vector<std::uint64_t> identities;
  identities.reserve(retained.size());
  for (const auto& state : retained) identities.push_back(state.state_identity);
  hooks.preselect(identities);
  for (std::size_t round = 0;; ++round) {
    if (round == 4096U)
      throw std::runtime_error("ordered drawing exceeds bounded rounds");
    bool more_work = false;
    for (const auto identity : identities) more_work = hooks.draw_round(identity) || more_work;
    if (!more_work) break;
  }
  hooks.restore_selected();
}

} // namespace off::graphics
