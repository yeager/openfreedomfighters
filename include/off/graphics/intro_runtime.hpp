#pragma once

#include "off/graphics/fresh_intro_camera.hpp"
#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/runtime/application_services.hpp"
#include "off/runtime/component_lifecycle.hpp"
#include "off/graphics/picture_color_state.hpp"
#include "off/graphics/picture_submission_cache.hpp"
#include "off/graphics/picture_view_transition.hpp"
#include "off/graphics/renderer_frame.hpp"
#include "off/graphics/renderer_frame_pass.hpp"
#include "off/graphics/picture_ordered_coordinator.hpp"
#include <map>
#include <memory>

namespace off::graphics {

struct IntroRuntimeHandle {
  std::uint64_t value{};
  bool operator==(const IntroRuntimeHandle&) const = default;
};

// Canonical record lease, not an audio channel. Owner binding is published by
// the explicit pre-hook; source construction must not mark this owner active.
class IntroRuntimeSound final {
public:
  [[nodiscard]] std::size_t source_index() const noexcept { return source_->directory_index; }
  [[nodiscard]] IntroRuntimeHandle handle() const noexcept { return handle_; }
  [[nodiscard]] audio::SoundRecord& record();
  [[nodiscard]] const audio::SoundRecord& record() const;
  [[nodiscard]] std::uint64_t owner_binding() const noexcept { return owner_binding_; }
  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
private:
  friend class IntroRuntime;
  const IntroPreparedSound* source_{};
  IntroRuntimeHandle handle_;
  audio::SoundRecordLease lease_;
  std::uint64_t owner_binding_{};
  bool active_{}, failed_{};
};

struct IntroSoundSpatialState {
  std::array<float,3> position;
  std::array<float,3> direction;
};

struct IntroSoundPreparationServices {
  // Required live resource services. These are not authored source flags or a
  // guessed camera transform. The last callback performs status |= 0x1 on the
  // actual owner only when the preceding live scene gate is true.
  // Callbacks must keep this host and its borrowed application alive. Recursive
  // preparation/stop through this host is rejected, not a mutation-safe traversal.
  std::function<std::uint32_t(IntroRuntimeHandle)> resource_flags;
  std::function<IntroRuntimeHandle(IntroRuntimeHandle)> parent_owner;
  std::function<IntroSoundSpatialState(IntroRuntimeHandle)> spatial_state;
  std::function<bool()> owner_enable_requested;
  std::function<void(IntroRuntimeHandle)> enable_owner;
};

class IntroRuntimePicture final {
public:
  [[nodiscard]] std::size_t source_index() const noexcept { return source_->directory_index; }
  [[nodiscard]] IntroRuntimeHandle handle() const noexcept { return handle_; }
  [[nodiscard]] std::uint32_t renderer_resource_id() const noexcept { return renderer_resource_id_; }
  [[nodiscard]] std::uint32_t source_flags() const noexcept { return source_flags_; }
  // Full factory/normalization lifecycle has not been established.
  [[nodiscard]] std::optional<std::uint32_t> runtime_resource_flags() const noexcept { return std::nullopt; }
  [[nodiscard]] PictureColorState& color_state() noexcept { return *colors_; }
  [[nodiscard]] const PictureColorState& color_state() const noexcept { return *colors_; }
  [[nodiscard]] std::span<const data::PictureResourceDescriptor> descriptors() const noexcept { return *descriptors_; }
  [[nodiscard]] data::PictureDrawPlan draw_plan() const {
    return colors_->draw_plan(source_->picture.draw_groups(), source_->bindings.entries());
  }
  [[nodiscard]] PictureSubmissionCache& submission_cache() noexcept { return cache_; }
private:
  friend class IntroRuntime;
  const IntroPreparedPicture* source_{};
  IntroRuntimeHandle handle_;
  std::uint32_t renderer_resource_id_{}, source_flags_{};
  std::vector<data::PictureResourceDescriptor>* descriptors_{};
  std::unique_ptr<PictureColorState> colors_;
  PictureSubmissionCache cache_;
};

// Stable scene ownership, not automatic cut admission or completed initialization.
// All identities and borrowed material/descriptor storage die with this host.
// The borrowed ApplicationServices must outlive this host and its bound callbacks.
// The borrowed scene component sequence must also outlive this host.
class IntroRuntime final {
public:
  IntroRuntime(IntroPreparedResources&& resources, runtime::ApplicationServices& application,
               runtime::SceneComponentSequence& component_sequence);
  IntroRuntime(const IntroRuntime&) = delete;
  IntroRuntime& operator=(const IntroRuntime&) = delete;
  IntroRuntime(IntroRuntime&&) = delete;
  IntroRuntime& operator=(IntroRuntime&&) = delete;
  [[nodiscard]] const IntroPreparedResources& resources() const noexcept { return resources_; }
  [[nodiscard]] runtime::ApplicationServices& application() noexcept { return application_; }
  [[nodiscard]] runtime::ComponentLifecycle& components() noexcept { return components_; }
  [[nodiscard]] const runtime::ComponentLifecycle& components() const noexcept { return components_; }
  [[nodiscard]] std::span<const std::unique_ptr<IntroRuntimeSound>> sounds() const noexcept { return sounds_; }
  [[nodiscard]] IntroRuntimeSound& sound_for_source(std::size_t source);
  // A concrete owner pre-hook, not the complete global traversal. All owner
  // pre-hooks must finish before either component phase; normal startup does
  // not call this until the live resource services exist. Failure poisons the
  // sound owner and reports unsupported disposal instead of leaving callbacks
  // able to continue as though owner destruction had succeeded.
  void prepare_sound_owner(std::size_t source, const IntroSoundPreparationServices& services);
  // Binding-stop request only, not the original deleting-disposal operation.
  void stop_sound_owner(std::size_t source);
  // Reader-completion/phase-one parameter operation on the SAME canonical
  // owner binding. No binding means no writes. Only the approved unchanged
  // intro Extend subset is supported; this is not lifecycle admission.
  void apply_sound_extension(std::size_t source);
  // Catalog membership includes unconstructed/removed entries, not a live
  // owner attachment collection suitable for runtime lookup or disposal.
  [[nodiscard]] std::span<const std::size_t> owner_components(IntroRuntimeHandle owner) const;
  // Component 0 describes the synthesized RootGroup; all authored attachments
  // follow in directory/attachment order. Catalog order is not construction.
  [[nodiscard]] std::size_t controller_component_index() const noexcept { return controller_component_; }
  // Caller still owes actual global lifecycle admission and external services.
  // Clock/audio resolve through the same application state retained by this scene.
  void run_controller_phase_two(const IntroControllerPhaseTwoServices& external);
  // Install on the concrete MovieControl instance constructed in components().
  [[nodiscard]] runtime::ComponentCallback controller_phase_two_callback(
      const IntroControllerPhaseTwoServices& external);
  [[nodiscard]] IntroControllerInitialization& controller_initialization() noexcept { return controller_initialization_; }
  [[nodiscard]] const IntroControllerInitialization& controller_initialization() const noexcept { return controller_initialization_; }
  [[nodiscard]] FreshIntroCamera& camera() noexcept { return camera_; }
  [[nodiscard]] const FreshIntroCamera& camera() const noexcept { return camera_; }
  [[nodiscard]] IntroRuntimeHandle root_handle() const noexcept { return {1}; }
  [[nodiscard]] IntroRuntimeHandle camera_root_owner() const noexcept { return root_handle(); }
  [[nodiscard]] IntroRuntimeHandle source_handle(std::size_t source) const;
  [[nodiscard]] std::optional<std::size_t> source_index(IntroRuntimeHandle handle) const;
  [[nodiscard]] std::span<const IntroRuntimeHandle> additional_owner_order() const noexcept { return additional_; }
  [[nodiscard]] const std::vector<PictureHierarchyNode>& hierarchy() const noexcept { return hierarchy_; }
  [[nodiscard]] std::uint32_t hierarchy_index(IntroRuntimeHandle handle) const;
  void set_local_transform(IntroRuntimeHandle handle, const std::array<float,9>& basis,
                           const std::array<float,3>& position);
  [[nodiscard]] std::span<const std::unique_ptr<IntroRuntimePicture>> pictures() const noexcept { return pictures_; }
  [[nodiscard]] IntroRuntimePicture& picture_for_source(std::size_t source);
  [[nodiscard]] std::uint32_t paired_material(std::uint32_t prm_offset) const;
  // Explicit bounded projection only: caller still owes input-map and generic
  // scheduling effects. Never invoked by construction or interpreted as ready.
  void project_selected_window_camera_state();
  [[nodiscard]] bool window_camera_projection_applied() const noexcept { return projected_; }
  [[nodiscard]] RendererFrameClock& frame_clock() noexcept { return clock_; }
  [[nodiscard]] RendererFrame& renderer_frame() noexcept { return frame_; }
  [[nodiscard]] RendererFramePass& frame_pass() noexcept { return frame_pass_; }
  [[nodiscard]] PictureOrderedCoordinator& ordered_coordinator() noexcept { return ordered_; }
  [[nodiscard]] PictureViewTransition& view_transition() noexcept { return view_; }
private:
  runtime::ApplicationServices& application_;
  IntroPreparedResources resources_;
  // Declared before components so their captures are destroyed before leases.
  std::vector<std::unique_ptr<IntroRuntimeSound>> sounds_;
  runtime::ComponentLifecycle components_;
  std::vector<std::vector<std::size_t>> owner_components_;
  std::size_t controller_component_{};
  IntroControllerInitialization controller_initialization_;
  FreshIntroCamera camera_;
  std::vector<PictureHierarchyNode> hierarchy_;
  std::vector<IntroRuntimeHandle> additional_;
  std::map<std::uint32_t, std::vector<data::PictureResourceDescriptor>> descriptors_;
  std::map<std::uint32_t, std::uint32_t> materials_;
  std::vector<std::unique_ptr<IntroRuntimePicture>> pictures_;
  RendererFrameClock clock_;
  RendererFrame frame_;
  RendererFramePass frame_pass_;
  PictureOrderedCoordinator ordered_;
  PictureViewTransition view_;
  bool projected_{false};
  bool sound_preparation_busy_{};
};
} // namespace off::graphics
