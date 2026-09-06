#pragma once

#include "off/graphics/fresh_intro_camera.hpp"
#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/runtime/application_services.hpp"
#include "off/runtime/component_lifecycle.hpp"
#include "off/runtime/scene_event_names.hpp"
#include "off/graphics/picture_color_state.hpp"
#include "off/graphics/picture_submission_cache.hpp"
#include "off/graphics/picture_view_transition.hpp"
#include "off/graphics/renderer_frame.hpp"
#include "off/graphics/renderer_frame_pass.hpp"
#include "off/graphics/renderer_camera_registry.hpp"
#include "off/graphics/preview_camera_component.hpp"
#include "off/graphics/root_group_component.hpp"
#include "off/graphics/picture_ordered_coordinator.hpp"
#include <map>
#include <memory>

namespace off::graphics {

class IntroRuntime;

struct IntroRuntimeHandle {
  std::uint64_t value{};
  bool operator==(const IntroRuntimeHandle&) const = default;
};
// Separate typed resource domain. Resources can exist before an owner is
// associated; tokens are native identities, not original allocator pointers.
struct IntroRuntimeResourceHandle {
  std::uint64_t value{};
  bool operator==(const IntroRuntimeResourceHandle&) const = default;
};
struct IntroRuntimeResourceState {
  std::uint32_t flags;
  IntroRuntimeResourceHandle context;
  // Common resource construction clears these fields; owner state is separate.
  std::uint32_t metadata{},directory_auxiliary{};
};
struct IntroSynthesizedCameraMetadata {
  std::string name;
  std::uint32_t class_identifier;
};
enum class IntroResourceLoadStage {
  prepared, constructing_root, root_ready, allocating_initial_scope, initial_scope_ready, first_group_ready, window_language_ready, picture_component_prefix_ready, failed
};
// Concrete constructor state, deliberately separate from the prepared asset view.
struct IntroConstructedPictureOwner {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::string name;
  std::uint32_t class_identifier{0x00200046U},packed_color{0xffffffffU},material_selector{},component_mask{};
  std::uint8_t alpha{255},alignment{0x11},exponent_control{0x80},submission_control{8};
  std::array<float,2> size_scale{1.0F,1.0F},alignment_offset{};
  bool backing_available{},submission_transform_dirty{},submission_cache_available{};
  std::vector<std::uint64_t> attachments;
};
struct IntroConstructedPictureComponent {
  IntroRuntimeHandle owner;
  std::int32_t attachment_argument{};
  std::optional<std::uint32_t> fade_start,fade_deadline,fade_state,fade_in_event,fade_out_event;
};
struct IntroAuthoredGroupOwner {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::string name;
  std::uint32_t class_identifier{0x00100001U},flags{0x03000000U},sentinel{0xffffffffU};
  float scalar{1.0F};
  std::uint32_t source_word{};
  std::uint32_t aggregate_flags{},component_mask{};
  IntroRuntimeHandle auxiliary{};
};
struct IntroDeferredReaderWork {
  IntroRuntimeResourceHandle resource;
  std::uint32_t source_offset;
};
struct IntroWindowOwner {
  IntroAuthoredGroupOwner group;
  IntroRuntimeHandle enclosing_window{},selected_camera{},cursor{},auxiliary{};
  std::vector<IntroRuntimeHandle> cameras;
  float input_scalar{1.0F},pending_visibility{0.0F};
  std::uint32_t input_mode{1},tracking_timer{},local_counter{};
  bool option_a{},option_b{},option_c{true},owned_action_map_cleanup{};
  bool input_suppression_held{true},local_input_tracking{true},auxiliary_terminal{};
  std::uint8_t tracking_sentinel{0xfe};
  // Storage precedes its lease so removal happens before the scalar dies.
  runtime::LiveVariableLease show_2d;
};
struct IntroSceneResourceProperty {
  std::uint32_t type{16};
  IntroRuntimeResourceHandle resource;
};
struct IntroSourceResourceScope {
  std::uint32_t count_group{};
  std::array<std::uint32_t,24> counts{};
  std::array<std::optional<std::uint32_t>,24> next_in_partition{};
  std::vector<IntroRuntimeResourceHandle> resources;
};
struct IntroRootOwnerState {
  std::string name;
  std::uint32_t class_identifier;
  std::uint32_t aggregate_flags{}, component_mask{};
  bool room_mode{}, enabled{};
  std::map<std::uint32_t,std::vector<IntroRuntimeResourceHandle>> category_memberships{};
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
  // The same optional live word used by resource mutations, never source flags.
  [[nodiscard]] std::optional<std::uint32_t> runtime_resource_flags() const;
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
  const IntroRuntime* runtime_{};
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
  // Executes the fresh ROOT/ZROOM and immediate RootGroup stage. Prepared
  // authored parent links are not live attachments and are detached here.
  // Source construction/attachment must follow against this same root.
  void construct_root();
  void prepare_source_event_names();
  [[nodiscard]] std::uint16_t declare_scene_event_name(std::string_view name,std::uint16_t requested=0);
  [[nodiscard]] const runtime::SceneEventNames& scene_event_names() const noexcept {return event_names_;}
  [[nodiscard]] std::span<const std::optional<std::uint32_t>> source_event_name_mapping() const noexcept {return source_event_name_mapping_;}
  // Explicit native cold-load staging before engine renderer creation. Reset
  // retained scene progress once, execute first-row progress, then allocate.
  // This is not evidence of the original cold reset caller or renderer timing.
  void begin_source_loading_without_engine_renderer();
  // Pre-row operation for the next unconstructed directory entry. Requires
  // genuinely absent engine renderer, not merely a failed readiness check.
  float advance_source_loading_progress_without_engine_renderer(std::size_t source);
  [[nodiscard]] std::optional<float> loading_progress() const noexcept {return loading_progress_;}
  // Actual first-scope batch only. Later scopes must be interleaved with real
  // owner construction and attachment; this never constructs all source rows.
  void allocate_initial_source_scope();
  void construct_first_authored_group();
  void construct_window_language_groups_without_engine_renderer();
  void construct_picture_component_prefix_without_engine_renderer();
  [[nodiscard]] const IntroConstructedPictureOwner* constructed_picture_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedPictureComponent* constructed_picture_component(std::size_t source) const noexcept;
  [[nodiscard]] const std::unique_ptr<IntroWindowOwner>& window_owner() const noexcept {return window_owner_;}
  [[nodiscard]] const std::optional<IntroAuthoredGroupOwner>& language_owner() const noexcept {return language_owner_;}
  [[nodiscard]] IntroRuntimeHandle current_source_parent() const noexcept {return current_source_parent_.value?current_source_parent_:root_handle();}
  [[nodiscard]] std::optional<IntroSceneResourceProperty> scene_resource_property(std::string_view key) const;
  void set_scene_resource_property_native(std::string key,IntroRuntimeResourceHandle resource);
  [[nodiscard]] const std::optional<IntroAuthoredGroupOwner>& first_authored_group() const noexcept {return first_authored_group_;}
  [[nodiscard]] std::uint32_t group_class_instance_count() const {return application_.group_class_instance_count();}
  [[nodiscard]] bool manager_row_edit() const noexcept {return manager_row_edit_;}
  [[nodiscard]] bool scene_resource_edit() const noexcept {return scene_resource_edit_;}
  [[nodiscard]] std::uint32_t count_group_selector() const noexcept {return count_group_selector_;}
  [[nodiscard]] std::span<const IntroRuntimeResourceHandle> loaded_resource_handles() const noexcept {return loaded_resource_handles_;}
  [[nodiscard]] std::span<const std::optional<IntroRuntimeResourceHandle>> directory_resource_mapping() const noexcept {return directory_resource_mapping_;}
  [[nodiscard]] std::span<const IntroDeferredReaderWork> deferred_reader_work() const noexcept {return deferred_reader_work_;}
  [[nodiscard]] std::span<const IntroSourceResourceScope> source_resource_scopes() const noexcept {return source_resource_scopes_;}
  [[nodiscard]] std::optional<IntroRuntimeResourceHandle> allocated_source_resource(std::size_t source) const;
  [[nodiscard]] IntroResourceLoadStage resource_load_stage() const noexcept {return resource_load_stage_;}
  [[nodiscard]] const std::optional<IntroRootOwnerState>& root_owner_state() const noexcept {return root_owner_state_;}
  [[nodiscard]] const RootGroupComponent* root_group() const noexcept {return root_group_.get();}
  [[nodiscard]] std::span<const std::size_t> root_attached_components() const noexcept {return root_attachments_;}
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
  [[nodiscard]] std::uint32_t resource_index(IntroRuntimeResourceHandle resource) const;
  [[nodiscard]] std::optional<IntroRuntimeHandle> associated_resource_owner(IntroRuntimeResourceHandle resource) const;
  [[nodiscard]] const std::optional<IntroRuntimeResourceState>& resource_state_for_handle(IntroRuntimeResourceHandle resource) const;
  [[nodiscard]] IntroRuntimeResourceHandle resource_parent(IntroRuntimeHandle owner) const;
  [[nodiscard]] std::vector<IntroRuntimeHandle> child_owners(IntroRuntimeHandle owner) const;
  [[nodiscard]] const std::optional<IntroRuntimeResourceState>& resource_state(IntroRuntimeHandle owner) const;
  // Publish complete live state from the actual resource loader/services. This
  // is NOT the original flag setter and runs none of its side effects. Source
  // flags or constructor constants must not stand in for post-load root state.
  void assign_resource_state(IntroRuntimeHandle owner,IntroRuntimeResourceState state);
  // Operate on established live hierarchy only; these do not execute loading
  // or attachment. Unknown ancestor words are rejected before any writes.
  void mutate_resource_low_byte(IntroRuntimeResourceHandle resource,std::uint32_t set_mask,std::uint32_t clear_mask);
  struct ResourceMutationModes {
    // Bind actual retained scene modes; neither is a constructor assumption.
    const bool& allocation_enabled;
    const bool& maintenance_suppressed;
  };
  // Unsupported maintenance rejects before any writes.
  void set_resource_flags_no_maintenance(IntroRuntimeResourceHandle resource,
      std::uint32_t set_mask,std::uint32_t clear_mask,
      ResourceMutationModes modes);
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
  std::shared_ptr<RootGroupComponent> root_group_;
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
  std::map<std::uint64_t,std::uint32_t> resource_indices_;
  std::vector<std::optional<IntroRuntimeResourceHandle>> hierarchy_resources_;
  std::vector<std::optional<IntroRuntimeHandle>> resource_owners_;
  std::vector<std::optional<IntroRuntimeResourceState>> resource_states_;
  IntroResourceLoadStage resource_load_stage_{IntroResourceLoadStage::prepared};
  runtime::SceneEventNames event_names_;
  std::vector<std::optional<std::uint32_t>> source_event_name_mapping_;
  bool source_event_names_prepared_{};
  std::optional<float> loading_progress_;
  void allocate_source_scope(std::uint32_t count_group);
  std::unique_ptr<IntroWindowOwner> window_owner_;
  std::optional<IntroAuthoredGroupOwner> language_owner_;
  std::map<std::size_t,IntroConstructedPictureOwner> constructed_picture_owners_;
  std::map<std::size_t,IntroConstructedPictureComponent> constructed_picture_components_;
  IntroRuntimeHandle current_source_parent_{};
  std::map<std::string,IntroSceneResourceProperty,std::less<>> scene_resource_properties_;
  std::optional<IntroRootOwnerState> root_owner_state_;
  std::vector<std::size_t> root_attachments_;
  bool resource_allocation_enabled_{}; // Actual scene-constructor mode starts off.
  std::vector<IntroSourceResourceScope> source_resource_scopes_;
  std::optional<IntroAuthoredGroupOwner> first_authored_group_;
  // Concrete native registration for the supported ZGROUP factory, independent
  // of authored class ordinals and component registration.
  std::uint32_t count_group_selector_{};
  bool manager_row_edit_{},scene_resource_edit_{};
  std::vector<IntroRuntimeResourceHandle> loaded_resource_handles_;
  std::vector<std::optional<IntroRuntimeResourceHandle>> directory_resource_mapping_;
  std::vector<IntroDeferredReaderWork> deferred_reader_work_;
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
    const auto found=owner_indices_.find(handle);
    return found!=owner_indices_.end() && resource_owners_[found->second]==IntroRuntimeHandle{handle};
  }
};
} // namespace off::graphics
