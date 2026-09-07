#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <vector>

namespace off::graphics {
class RendererCameraRegistry;
struct RegisteredCamera {std::uint64_t owner;float key;};
struct CameraRegistrationServices {
  std::function<bool(std::uint64_t)> live_owner;
  std::function<void(std::uint64_t)> notify_dimensions;
  std::function<bool()> backend_ready;
  // Required only when the actual renderer backend is present and ready.
  std::function<void(std::uint64_t)> admit_view;
};

// This remains separate from camera membership.  It models only the checked
// renderer-state boundary reached after a live camera has already entered the
// registry; it neither creates a renderer nor decodes renderer source data.
struct RendererViewRectangle {
  std::int32_t left{}, top{}, width{}, height{};
  bool operator==(const RendererViewRectangle&) const = default;
};
struct RendererViewState { std::uint64_t value{}; };
struct RendererCameraViewAdmissionServices {
  std::function<bool()> renderer_has_backend;
  std::function<bool()> renderer_backend_ready;
  std::function<std::optional<RendererViewState>()> state_zero;
  std::function<std::int32_t()> application_width;
  std::function<std::int32_t()> application_height;
  std::function<RendererViewState(RendererViewRectangle)> create_state_zero;
  std::function<bool(RendererViewState)> state_ready;
  // Counts belong to the concrete state host.  The admission boundary checks
  // them before requesting mutation; it never uses an unbounded placeholder.
  std::function<std::size_t(RendererViewState)> pending_count;
  std::function<void(RendererViewState,std::uint64_t,std::int32_t)> queue_pending;
  std::function<std::size_t(RendererViewState)> admitted_view_count;
  std::function<std::uint64_t(RendererViewState,std::uint64_t)> allocate_view;
  std::function<void(std::uint64_t,std::uint64_t)> associate_camera_intermediate;
  std::function<void(std::uint64_t)> register_backend_records;
  std::function<void(std::uint64_t,std::int64_t)> insert_view;
  std::function<void(std::uint64_t)> increment_view_use;
  std::function<void(RendererViewState)> renumber_view_ordinals;
};

// The caller needs this distinction to preserve the recovered first-cut
// ordering: an unavailable backend is not a queued camera, and a queued
// camera is not a view that can already be traversed.
enum class RendererCameraViewAdmissionResult {
  backend_absent,
  backend_not_ready,
  pending_queued,
  view_admitted,
};

// An explicit post-loader admission service.  A backend that is absent or not
// ready deliberately has no state/pending/view side effect.  Once backend
// admission begins, a missing concrete state or allocation service is a
// failure, never a placeholder state or successful present.
class RendererCameraViewAdmission final {
public:
  RendererCameraViewAdmissionResult admit(
      std::uint64_t camera, std::int32_t camera_priority,
      const RendererCameraViewAdmissionServices& services);
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  bool busy_{}, failed_{};
};

// A state retains cameras admitted while it is non-ready.  It deliberately
// stores only identities/priorities and delegates real view work back through
// the checked ready-admission route.  This is not a renderer state allocator.
struct RendererPendingCamera {
  std::uint64_t camera{};
  std::int32_t priority{};
  bool operator==(const RendererPendingCamera&) const = default;
};
struct RendererPendingMaterializationServices {
  std::function<bool(RendererViewState)> initialize_state;
  std::function<void(std::uint64_t,std::int32_t)> admit_ready_camera;
};
class RendererPendingCameraQueue final {
public:
  static constexpr std::size_t capacity=16;
  void append(RendererViewState state,std::uint64_t camera,std::int32_t priority);
  void materialize(RendererViewState state,const RendererPendingMaterializationServices& services);
  [[nodiscard]] std::vector<RendererPendingCamera> entries() const;
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  std::optional<RendererViewState> state_;
  std::vector<RendererPendingCamera> entries_;
  bool busy_{},failed_{};
};

// This is the one reviewed renderer-initialization replay, separate from
// camera registration and state materialization.  It does not decode a
// renderer resource or establish a backend frame/present path.
struct RendererRegistryReplayCamera { std::uint64_t camera{}; std::int32_t priority{}; };
struct RendererRegistryReplayServices {
  std::function<bool()> setup_renderer;
  std::function<bool()> backend_ready_before_initialization;
  std::function<bool()> initialize_backend;
  std::function<std::optional<RendererViewState>()> state_zero;
  std::function<std::optional<RendererRegistryReplayCamera>(std::uint64_t)> resolve_live_camera;
  std::function<void(std::uint64_t,std::int32_t)> admit_camera;
};
class RendererRegistryReplay final {
public:
  void initialize(RendererCameraRegistry& registry,const RendererRegistryReplayServices& services);
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  bool busy_{},failed_{};
};
// Canonical renderer membership, distinct from backend states/views. Stable
// owner services and no reentry are native policies. Post-insertion failures
// preserve their prefix and poison the registry; they are not retryable success.
class RendererCameraRegistry final {
public:
  RendererCameraRegistry()=default;
  RendererCameraRegistry(const RendererCameraRegistry&)=delete;
  RendererCameraRegistry& operator=(const RendererCameraRegistry&)=delete;
  void register_camera(std::uint64_t owner,float key,const CameraRegistrationServices& services);
  [[nodiscard]] std::uint64_t camera_at(std::size_t index,
      const std::function<bool(std::uint64_t)>& live_owner);
  // Snapshot for diagnostics/explicit callers, not a replacement for live sweeps.
  [[nodiscard]] std::vector<RegisteredCamera> entries() const;
  // Stable non-pruning traversal for the renderer initialization replay.
  // Callers may resolve stale entries but must not mutate this registry.
  void visit_retained(const std::function<void(const RegisteredCamera&)>& visitor);
  [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
  std::list<RegisteredCamera> entries_;
  std::optional<std::list<RegisteredCamera>::iterator> last_inserted_;
  bool busy_{},failed_{};
  void check_idle() const;
};
} // namespace off::graphics
