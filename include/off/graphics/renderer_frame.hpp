#pragma once

#include <cstdint>
#include <functional>

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

} // namespace off::graphics
