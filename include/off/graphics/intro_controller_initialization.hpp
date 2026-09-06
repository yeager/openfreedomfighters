#pragma once

#include "off/graphics/picture_view_transition.hpp"
#include <functional>
#include <string_view>

namespace off::graphics {

enum class IntroPresentationResult { presented, device_lost, configuration_reset_required };

// Required live services for the controller's admitted global phase-two callback.
// Global property queries receive zero-initialized outputs and must leave them
// unchanged on a missing key. This is NOT the scene's camera/reference store.
// Renderer identities must stay live through the synchronous invocation; their
// position as the first renderer may change between calls. No structural or
// recursive initialization is supported by this native boundary.
struct IntroControllerPhaseTwoServices {
  std::function<bool()> input_manager_exists;
  std::function<void()> register_movie_control_action_map;
  std::function<void(bool)> assign_engine_clock_mode;
  std::function<void(std::string_view,std::uint32_t&)> query_global_property;
  std::function<std::int32_t()> current_audio_volume;
  std::function<void(std::uint32_t)> request_audio_volume;
  std::function<std::uint32_t()> scene_integer_clock;
  std::function<std::uint64_t()> first_renderer;
  std::function<std::int32_t(std::uint64_t)> renderer_height;
  std::function<std::int32_t(std::uint64_t)> renderer_width;
  std::function<void(std::uint64_t,const PictureDeviceViewport&)> set_viewport;
  std::function<bool(std::uint64_t)> renderer_has_stencil;
  std::function<void(std::uint64_t,const PictureViewClear&)> clear;
  // This is device presentation, not RendererFrame or scene traversal. The
  // service owes applicable profiling. Only healthy presentation is supported;
  // device loss / pending configuration reset must not be reported as presented.
  std::function<IntroPresentationResult(std::uint64_t)> present;
};

class IntroControllerInitialization final {
public:
  IntroControllerInitialization() = default;
  IntroControllerInitialization(const IntroControllerInitialization&) = delete;
  IntroControllerInitialization& operator=(const IntroControllerInitialization&) = delete;
  // Constructor deadline is zero, not proof phase two ran or update is admitted.
  [[nodiscard]] std::uint32_t deadline() const noexcept { return deadline_; }
  [[nodiscard]] bool deadline_assigned() const noexcept { return deadline_assigned_; }
  // Records successful assignment through the global engine service, not the
  // current global mode: other engine callers may subsequently change it.
  [[nodiscard]] bool clock_mode_assignment_completed() const noexcept { return mode_assigned_; }
  [[nodiscard]] bool phase_two_completed() const noexcept { return completed_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  // Caller owes global lifecycle owner/mask/status admission. No status bits,
  // scene clock, renderer frame clock, readiness or activation are fabricated.
  // Preflight rejects missing services without effects. Callback failure retains
  // every completed prefix and disallows retry; no rollback/presentation recovery.
  // A successfully completed callback may be explicitly invoked again: the global
  // phase-two pass has no completed-status guard. Previous deadline/assignment
  // remain observable until their corresponding new operation completes.
  void run_phase_two(const IntroControllerPhaseTwoServices& services);
private:
  std::uint32_t deadline_{};
  bool deadline_assigned_{}, mode_assigned_{}, completed_{}, running_{}, failed_{};
};
} // namespace off::graphics
