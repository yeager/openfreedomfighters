#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// One shared word per engine, not per renderer, view, application update or
// scene. Fresh engine construction starts at one. Adoption of an existing word
// is an explicit native boundary; it does not assert a first-intro frame value.
class RendererFrameClock final {
public:
  RendererFrameClock() = default;
  explicit RendererFrameClock(std::uint32_t current_frame) : frame_(current_frame) {}
  RendererFrameClock(const RendererFrameClock&) = delete;
  RendererFrameClock& operator=(const RendererFrameClock&) = delete;
  RendererFrameClock(RendererFrameClock&&) = delete;
  RendererFrameClock& operator=(RendererFrameClock&&) = delete;
  [[nodiscard]] std::uint32_t value() const noexcept { return frame_; }
private:
  friend class RendererFrame;
  friend class EligibleRendererFrame;
  std::uint32_t frame_{1};
  bool coordinating_{false};
};

enum class RendererFrameOutcome { skipped, admission_failed, rendered };

struct RendererFrameLifecycleHooks {
  std::function<bool()> admit_device_scene;
  std::function<void()> backend_traversal;
  std::function<void()> admitted_post_render;
  std::function<void()> end_scene;
  std::function<void()> renderer_completion;
};

// Outer renderer-frame lifecycle, distinct from RendererFramePass's inner
// backend/state traversal. No backend success, scene readiness or running gate
// is inferred. Different renderer instances share the same engine clock.
class RendererFrame final {
public:
  RendererFrame() = default;
  RendererFrame(const RendererFrame&) = delete;
  RendererFrame& operator=(const RendererFrame&) = delete;
  RendererFrame(RendererFrame&&) = delete;
  RendererFrame& operator=(RendererFrame&&) = delete;

  // Failed outer gates do nothing, including no hook validation or increment.
  // Otherwise all hooks are required before effects, even if admission later
  // returns false (native validation policy). Completion precedes the wrapping
  // uint32 increment on both admitted and failed-admission paths.
  // Stable callback/clock lifetimes are required. Reentry through this instance
  // or another instance sharing its clock rejects even when nested gates fail.
  // Callback exceptions retain their completed prefix, without synthesizing
  // later completion/end-scene/increment. Abort that frame; no rollback or
  // transactional retry is provided. Guards release when the invocation exits.
  [[nodiscard]] RendererFrameOutcome run(RendererFrameClock& clock,
      bool engine_running, bool renderer_initialized,
      const RendererFrameLifecycleHooks& hooks);
private:
  bool running_{false};
};

// A bounded, disconnected model of the ordinary scene-update prefix.  The
// renderer chain is deliberately followed callback-then-next: a callback sees
// its current node before the next link is read.  Callers must provide a finite
// stable chain for the duration of this call; the bound only makes that native
// safety policy explicit and is not an ownership model for the original.
struct LiveRendererNode {
  std::uint64_t identity{};
  LiveRendererNode* next{};
};

struct OrdinarySceneUpdateHooks {
  std::function<void()> scheduled_events;
  std::function<void()> ordinary_components;
  std::function<void()> position_and_bounds;
  std::function<void()> queued_maintenance;
  std::function<void(LiveRendererNode&)> renderer_frame;
  std::function<void()> post_render;
};

class OrdinarySceneUpdate final {
public:
  // All non-render services are required for every ordinary update.  Renderer
  // traversal and its post-render suffix are required only when render is true.
  // No gate here admits a camera, state, backend, device, draw, or present.
  void run(bool render, LiveRendererNode* renderer_head,
           const OrdinarySceneUpdateHooks& hooks);
private:
  bool running_{false};
};

// Frame-level lifecycle with the reviewed distinction between failed outer
// initialization gates and a failed device-scene admission.  The two local
// counters are reset only after the outer gates pass.  They are exposed as
// service-owned storage rather than being mistaken for the shared engine word.
struct EligibleRendererFrameState {
  std::uint32_t local_counter_a{};
  std::uint32_t local_counter_b{};
  // True only between successful device admission and EndScene completion.
  // An exception preserves this reached prefix for the caller to recover.
  bool in_scene{};
};

struct EligibleRendererFrameHooks {
  std::function<bool()> device_suppressed;
  std::function<bool()> backend_ready;
  std::function<bool()> begin_scene;
  std::function<void()> backend_traversal;
  std::function<void()> admitted_post_render;
  std::function<void()> end_scene;
  std::function<void()> renderer_completion;
};

class EligibleRendererFrame final {
public:
  [[nodiscard]] RendererFrameOutcome run(RendererFrameClock& clock,
      EligibleRendererFrameState& state, bool engine_running,
      bool renderer_initialized, const EligibleRendererFrameHooks& hooks);
private:
  bool running_{false};
};

// The backend model consumes a matching-state snapshot once.  It then runs all
// state/view collection phases, all maintenance phases, backend preparation,
// and drawing rounds in that order.  This is a service boundary only: no state
// matching, view allocation, draw submission, clear, or presentation is
// inferred from successful callback return.
struct BackendAdmittedView {
  std::uint64_t identity{};
  std::optional<std::uint64_t> camera_identity;
};

struct BackendStateInput {
  std::uint64_t renderer_identity{};
  std::uint64_t state_identity{};
  std::span<const BackendAdmittedView> views;
};

struct BackendTraversalHooks {
  std::function<void(std::uint64_t)> state_frame_begin;
  std::function<bool(std::uint64_t)> camera_enabled;
  std::function<void(std::uint64_t, std::uint64_t)> begin_view_transform;
  std::function<void(std::uint64_t, std::uint64_t)> begin_view;
  std::function<void(std::uint64_t, std::uint64_t)> common_view;
  std::function<void(std::uint64_t, std::uint64_t)> end_view;
  std::function<void(std::uint64_t)> state_maintenance;
  std::function<void()> backend_prepare;
  std::function<void(std::span<const std::uint64_t>)> preselect;
  // Returns whether another whole-state round is needed.  It is called for
  // every retained state, including empty/exhausted ones, in every round.
  std::function<bool(std::uint64_t)> draw_round;
  std::function<void()> restore_selected;
};

class BackendTraversal final {
public:
  void run(std::uint64_t renderer_identity,
           std::span<const BackendStateInput> states,
           const BackendTraversalHooks& hooks);
private:
  bool running_{false};
};

} // namespace off::graphics
