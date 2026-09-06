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
#include "off/graphics/renderer_camera_registry.hpp"
#include "off/graphics/preview_camera_component.hpp"
#include "off/graphics/picture_ordered_coordinator.hpp"
#include <map>
#include <memory>

namespace off::graphics {

struct IntroRuntimeHandle {
  std::uint64_t value{};
  bool operator==(const IntroRuntimeHandle&) const = default;
};
// Separate typed resource domain. Native resource/owner pairs share the numeric
// token, not their semantics; this is not an original allocator representation.
struct IntroRuntimeResourceHandle {
  std::uint64_t value{};
  bool operator==(const IntroRuntimeResourceHandle&) const = default;
};
struct IntroRuntimeResourceState {
  std::uint32_t flags;
  IntroRuntimeResourceHandle context;
};
struct IntroSynthesizedCameraMetadata {
  std::string name;
  std::uint32_t class_identifier;
};
struct IntroCameraRegistrationServices {
  std::function<std::int32_t()> width,height;
  std::function<bool()> backend_ready;
  std::function<void(IntroRuntimeHandle)> admit_view;
};
struct IntroSoundListener {
  IntroRuntimeHandle owner,context;
};
struct IntroOrdinaryFrameServices {
  std::function<bool()> paused;
  std::function<std::optional<std::uint64_t>()> component_filter;
  std::function<PreviewCameraInput()> preview_input;
  std::function<void(IntroRuntimeResourceHandle)> enqueue_transform;
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
  [[nodiscard]] runtime::OrdinaryComponentManager* ordinary_components() noexcept {return ordinary_.get();}
  [[nodiscard]] const runtime::OrdinaryComponentManager* ordinary_components() const noexcept {return ordinary_.get();}
  // Called by actual enable/admission after admitted ordinary bit is set.
  // Lazily creates the scene manager; querying it above never creates it.
  void register_ordinary_component(std::size_t index);
  [[nodiscard]] std::uint64_t component_handle(std::size_t index) const;
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
  // Explicit real renderer membership for the retained authored camera. Does
  // not activate a cut, fabricate backend readiness or synthesize DefaultCam.
  void register_camera(float key,const IntroCameraRegistrationServices& services);
  void register_camera(IntroRuntimeHandle owner,float key,const IntroCameraRegistrationServices& services);
  [[nodiscard]] FreshIntroCamera& camera_for_owner(IntroRuntimeHandle owner);
  [[nodiscard]] const FreshIntroCamera& camera_for_owner(IntroRuntimeHandle owner) const;
  [[nodiscard]] RendererCameraRegistry& registered_cameras() noexcept {return registered_cameras_;}
  [[nodiscard]] IntroRuntimeHandle camera_context() const noexcept {return camera_context_;}
  void set_camera_context(IntroRuntimeHandle context);
  void set_camera_context(IntroRuntimeHandle owner,IntroRuntimeHandle context);
  [[nodiscard]] IntroRuntimeHandle camera_context(IntroRuntimeHandle owner) const;
  void set_sound_listener(IntroRuntimeHandle owner);
  [[nodiscard]] std::optional<IntroSoundListener> sound_listener();
  [[nodiscard]] IntroRuntimeHandle root_handle() const noexcept { return {owner_base_}; }
  [[nodiscard]] IntroRuntimeHandle camera_root_owner() const noexcept { return root_handle(); }
  [[nodiscard]] IntroRuntimeHandle source_handle(std::size_t source) const;
  [[nodiscard]] std::optional<std::size_t> source_index(IntroRuntimeHandle handle) const;
  [[nodiscard]] std::span<const IntroRuntimeHandle> additional_owner_order() const noexcept { return additional_; }
  [[nodiscard]] const std::vector<PictureHierarchyNode>& hierarchy() const noexcept { return hierarchy_; }
  [[nodiscard]] std::uint32_t hierarchy_index(IntroRuntimeHandle handle) const;
  [[nodiscard]] IntroRuntimeResourceHandle resource_handle(IntroRuntimeHandle owner) const;
  [[nodiscard]] IntroRuntimeHandle resource_owner(IntroRuntimeResourceHandle resource) const;
  [[nodiscard]] IntroRuntimeResourceHandle resource_parent(IntroRuntimeHandle owner) const;
  [[nodiscard]] std::vector<IntroRuntimeHandle> child_owners(IntroRuntimeHandle owner) const;
  [[nodiscard]] const std::optional<IntroRuntimeResourceState>& resource_state(IntroRuntimeHandle owner) const;
  // Publish complete live state from the actual resource loader/services. This
  // is NOT the original flag setter and runs none of its side effects. Source
  // flags or constructor constants must not stand in for post-load root state.
  void assign_resource_state(IntroRuntimeHandle owner,IntroRuntimeResourceState state);
  // Explicit first part of the loader fallback: query existing camera, construct
  // and attach a fresh child, set loader flag/pose and call the real queue. Root
  // resource state must already be known. Preview attachment, priority and
  // renderer registration follow later; this does not complete loader admission.
  [[nodiscard]] std::optional<IntroRuntimeHandle> create_default_camera_resource(
      bool single_allocation_mode,const std::function<void(IntroRuntimeResourceHandle)>& enqueue_transform);
  [[nodiscard]] std::optional<IntroRuntimeHandle> default_camera_handle() const noexcept {return default_camera_;}
  [[nodiscard]] const std::optional<IntroSynthesizedCameraMetadata>& default_camera_metadata() const noexcept {return default_camera_metadata_;}
  [[nodiscard]] bool default_camera_failed() const noexcept {return default_camera_failed_;}
  void attach_default_preview_camera();
  void finish_default_camera_registration(const IntroCameraRegistrationServices& services);
  [[nodiscard]] std::optional<IntroRuntimeHandle> ensure_default_camera(bool single_allocation_mode,
      const std::function<void(IntroRuntimeResourceHandle)>& enqueue_transform,
      const IntroCameraRegistrationServices& registration);
  [[nodiscard]] std::optional<std::size_t> default_preview_component_index() const noexcept {return default_preview_component_;}
  [[nodiscard]] std::span<const std::size_t> default_camera_components() const noexcept {return default_camera_attachments_;}
  [[nodiscard]] std::uint32_t default_camera_component_mask() const noexcept {return default_component_mask_;}
  // Scheduled events precede this call. Complete global lifecycle is mandatory;
  // unknown admitted concrete components fail, never receive no-op callbacks.
  void run_ordinary_components(const IntroOrdinaryFrameServices& services);
  // Borrow the actual hierarchy pose and complete resource flags, never a
  // separate preview/listener copy. No structural host mutation during use.
  [[nodiscard]] PreviewCameraResourceView camera_resource_view(IntroRuntimeHandle owner);
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
  std::uint64_t owner_base_{};
  IntroPreparedResources resources_;
  // Declared before components so their captures are destroyed before leases.
  std::vector<std::unique_ptr<IntroRuntimeSound>> sounds_;
  runtime::ComponentLifecycle components_;
  std::unique_ptr<runtime::OrdinaryComponentManager> ordinary_;
  std::vector<std::vector<std::size_t>> owner_components_;
  std::size_t controller_component_{};
  IntroControllerInitialization controller_initialization_;
  FreshIntroCamera camera_;
  RendererCameraRegistry registered_cameras_;
  IntroRuntimeHandle camera_context_; // The same synthesized root, not resource parent.
  std::vector<PictureHierarchyNode> hierarchy_;
  std::vector<IntroRuntimeHandle> hierarchy_owners_;
  std::map<std::uint64_t,std::uint32_t> owner_indices_;
  std::vector<std::optional<IntroRuntimeResourceState>> resource_states_;
  std::optional<IntroRuntimeHandle> default_camera_;
  std::optional<IntroSynthesizedCameraMetadata> default_camera_metadata_;
  std::unique_ptr<FreshIntroCamera> default_camera_owner_;
  IntroRuntimeHandle default_camera_context_;
  bool default_camera_busy_{},default_camera_failed_{},default_camera_registered_{};
  std::shared_ptr<PreviewCameraComponent> default_preview_;
  std::optional<std::size_t> default_preview_component_;
  std::vector<std::size_t> default_camera_attachments_;
  std::uint32_t default_component_mask_{};
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
  [[nodiscard]] bool live_owner(std::uint64_t handle) const noexcept {
    return owner_indices_.contains(handle);
  }
};
} // namespace off::graphics
