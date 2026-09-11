#pragma once

#include "off/graphics/picture_view_transition.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
  // Shape-only check used when retaining services; does not invoke them or
  // establish that their external state is ready for initialization.
  static void validate_services(const IntroControllerPhaseTwoServices& services);
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

// The reviewed MovieControl route is kept separate from the older bootstrap
// controller boundary above.  In particular this model does not imply a ready
// sound, a selected camera, a renderer view, or a presented frame.
struct MovieControlPhaseTwoServices {
  std::function<bool()> input_manager_exists;
  std::function<void()> register_action_map;
  std::function<void(bool)> assign_engine_clock_mode;
  // The real helper owns property reads and the audio-service request.  It is
  // deliberately an explicit dependency rather than an empty success path.
  std::function<void()> setup_audio_volume;
  std::function<std::int32_t()> scene_integer_clock;
  // This boundary owns the first-renderer lookup, dimensions, and reviewed
  // setup/clear calls.  It does not promise a device, frame, or presentation.
  std::function<void()> setup_and_clear_first_renderer;
};

struct MovieControlEvent16Services {
  // Manager admission inputs are captured for this dispatch pass.
  std::function<bool()> component_is_live;
  std::function<bool()> event16_enrolled;
  std::function<bool()> paused;
  std::function<std::optional<std::uint64_t>()> captured_component_filter;
  // This is the component wrapper's lifecycle gate.  Phase two is intentionally
  // not an independent event-16 gate.
  std::function<bool()> phase_one_completed;
  std::function<std::int32_t()> scene_integer_clock;
  // The preparation prefix must complete before the activation latch changes.
  std::function<void()> prepare_sequence_resources;
  // Both direct operations are synchronous service boundaries.  They neither
  // select a camera nor create a render view in this model.
  std::function<void(std::uint64_t)> send_cut_sequence_start;
  std::function<void(std::uint64_t)> send_group_state_requests;
};

enum class MovieControlEvent16Result {
  skipped_not_live,
  skipped_not_enrolled,
  skipped_paused,
  skipped_filtered,
  phase_one_incomplete,
  waiting_for_deadline,
  activated,
  already_active,
};

// A strict, disconnected model of MovieControl's phase-two callback and its
// first admitted ordinary event 16.  The caller supplies actual scene handles
// and delay units; there is no wall-clock fallback or synthetic readiness.
class MovieControlFirstUpdate final {
public:
  MovieControlFirstUpdate(std::uint64_t component_handle,std::uint64_t owner_handle,
                          std::int32_t delay);
  void run_phase_two(const MovieControlPhaseTwoServices& services);
  [[nodiscard]] MovieControlEvent16Result dispatch_event16(
      const MovieControlEvent16Services& services);
  [[nodiscard]] std::int32_t deadline() const noexcept { return deadline_; }
  [[nodiscard]] bool deadline_assigned() const noexcept { return deadline_assigned_; }
  [[nodiscard]] bool phase_two_callback_returned() const noexcept { return phase_two_returned_; }
  [[nodiscard]] bool activated() const noexcept { return activated_; }
  [[nodiscard]] std::optional<std::int32_t> playback_baseline() const noexcept { return playback_baseline_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  std::uint64_t component_handle_{},owner_handle_{};
  std::int32_t delay_{},deadline_{};
  std::optional<std::int32_t> playback_baseline_;
  bool deadline_assigned_{},phase_two_returned_{},activated_{},phase_two_running_{},event_running_{},failed_{};
};

// The first cut's camera path is intentionally independent of MovieControl's
// deadline/event gate.  It models only the synchronous CutSequence_Start
// receiver after that gate has admitted delivery; it is not a camera decoder,
// scene loader, renderer view, or audio-start implementation.
struct FirstCutRequestedCamera {
  std::uint64_t reference{};
  std::uint64_t runtime_owner{};
  std::uint32_t packed_background{};
  std::int32_t priority{};
  bool camera_class{};
};
struct FirstCutRegisteredCamera { std::uint64_t runtime_owner{}; std::string name; };
struct FirstCutRequestedCameraServices {
  // Missing means the same zero word as a present four-byte zero.  Any present
  // value of another size is rejected before branch effects.
  std::function<std::optional<std::vector<std::uint8_t>>(std::string_view)> read_scene_property;
  std::function<std::optional<FirstCutRequestedCamera>(std::uint64_t)> resolve_requested_camera;
  // The nonzero property route must resolve this separately; it may not reuse
  // the requested reference or substitute the zero-property route.
  std::function<bool(std::uint32_t)> resolve_named_camera;
  std::function<void(std::uint32_t,const FirstCutRequestedCamera&)> run_named_camera_route;
  std::function<void()> increment_renderer_control;
  std::function<void(bool)> assign_renderer_optional_flag;
  // Host supplies a stable, non-reentrant live walk.  It must not be a snapshot
  // disguised as one and must preserve retained registry membership.
  std::function<void(const std::function<void(const FirstCutRegisteredCamera&)>&)> visit_registered_cameras_live;
  std::function<std::optional<FirstCutRegisteredCamera>(std::uint64_t)> resolve_registered_camera;
  std::function<std::string()> current_scene_identifier;
  std::function<void(std::uint64_t)> disable_camera;
  std::function<void(std::uint32_t)> assign_scene_root_background;
  // The registration helper deduplicates by runtime owner and handles its own
  // inner dimension notification.  Its result is true only for a new entry.
  std::function<bool(std::uint64_t,float)> register_first_renderer_camera;
  // Cut setup invokes this outer notification for both new and existing entries.
  std::function<void(std::uint64_t)> notify_renderer_dimensions;
  // Optional only when no audio manager exists.
  std::function<void(std::uint64_t)> notify_audio_camera;
};
enum class FirstCutRequestedCameraResult { requested_camera_selected, named_camera_route };

class FirstCutRequestedCameraRoute final {
public:
  FirstCutRequestedCameraRoute(bool first_option,bool optional_renderer_flag,
                               std::uint64_t requested_reference);
  FirstCutRequestedCameraResult run(const FirstCutRequestedCameraServices& services);
  [[nodiscard]] std::optional<std::uint64_t> selected_reference() const noexcept { return selected_reference_; }
  // True only after this synchronous receiver mutation returned successfully;
  // it is an ordering marker for a later renderer walk in the same update.
  [[nodiscard]] bool renderer_walk_can_observe_mutation() const noexcept { return same_update_visible_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  bool first_option_{},optional_renderer_flag_{},running_{},failed_{},same_update_visible_{};
  std::uint64_t requested_reference_{};
  std::optional<std::uint64_t> selected_reference_;
};
} // namespace off::graphics
