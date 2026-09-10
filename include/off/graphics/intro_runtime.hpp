#pragma once

#include "off/graphics/fresh_intro_camera.hpp"
#include "off/cutscene/first_cut_player_initialization.hpp"
#include "off/graphics/intro_prepared_resources.hpp"
#include "off/data/keys_backing_evaluator.hpp"
#include "off/data/first_cut_component_payload_session.hpp"
#include "off/data/scene_lifetime_keys_registry.hpp"
#include "off/graphics/intro_named_global_section_envelope.hpp"
#include "off/graphics/intro_renderer_relocation_prefix.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/graphics/intro_lifecycle_admission.hpp"
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
#include "off/graphics/position_update_service.hpp"
#include "off/graphics/root_group_component.hpp"
#include "off/graphics/picture_ordered_coordinator.hpp"
#include "off/graphics/center_picture_position.hpp"
#include "off/cutscene/picture_activation_prefix.hpp"
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
  std::uint32_t metadata{};
  // Actual resource renderer identifier, seeded by the directory's complete
  // second word. Not a BUF offset, prepared draw ID or relocated owner token.
  std::uint32_t directory_auxiliary{};
};
struct IntroSynthesizedCameraMetadata {
  std::string name;
  std::uint32_t class_identifier;
};
enum class IntroResourceLoadStage {
  prepared, constructing_root, root_ready, allocating_initial_scope, initial_scope_ready, first_group_ready, window_language_ready, picture_component_prefix_ready, authored_camera_ready, second_window_picture_ready, second_window_scope_ready, following_visual_scope_ready, room_animation_scope_ready, lens_flare_animation_scope_ready, directory_construction_complete, failed
};
enum class IntroSoundLoadPolicy {prepared_compatibility,directory_construction};
// This is the boundary around the deferred reader queue, not evidence that a
// reader, reference translation, activation, or global lifecycle has run.
enum class IntroReaderBracketStage {
  not_started,
  restore_mode_selected,
  ordinary_reader_boundary_complete,
  failed
};
// This describes only the verified outer-loader tail boundaries.  It is not a
// claim that named/global data, renderer associations, or component lifecycle
// semantics have been implemented by this host.
enum class IntroOuterLoaderTailStage {
  not_started,
  source_lease_released,
  camera_zero_complete,
  first_saved_pass_complete,
  second_saved_pass_complete,
  incomplete,
  failed
};
struct IntroConstructedCameraOwner {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::string name;
  std::uint32_t class_identifier{0x00400003U},notification_sequence{};
};
struct IntroOwnerAuxiliary {
  std::vector<std::uint64_t> attachments;
  std::uint32_t component_mask{};
  std::span<const std::byte> borrowed_property_data;
  std::vector<std::byte> owned_property_data;
  bool work_available{};
};
struct IntroLiveCameraOwner {
  IntroConstructedCameraOwner metadata;
  FreshIntroCamera camera;
  IntroRuntimeHandle context;
  std::unique_ptr<IntroOwnerAuxiliary> auxiliary;
};
struct IntroVertAnimLocalState {
  std::array<std::uint32_t,2> zero_words{};
  std::array<std::uint16_t,2> zero_shorts{};
};
struct IntroMatPosAnimLocalState {
  std::array<std::uint32_t,3> backing_words{};
  std::uint16_t tracking_short{};
  std::array<std::uint32_t,2> tracking_words{};
};
struct IntroAnimationConstructionState {
  std::optional<IntroVertAnimLocalState> vert;
  std::optional<IntroMatPosAnimLocalState> mat;
  float rate{25};
  std::optional<float> scalar;
  bool enabled_a{true},enabled_b{true},extra_control{};
  std::optional<bool> enabled_c;
  std::uint16_t short_control{1};
  std::optional<std::uint32_t> word_control;
  std::optional<std::array<float,9>> basis;
  std::optional<std::array<float,3>> transient_vector;
};
struct IntroCutListConstructionState {
  // Opaque references, not fabricated scene resource handles. Their distinct
  // null constructor values differ from unproduced source/cursor fields.
  std::array<std::uint64_t,3> selected_references{};
  std::array<bool,7> controls{};
  std::uint32_t playback_clock{};
  float derived_end{0.0F};
  std::int32_t playback_tracking{};
  std::optional<float> optional_scalar;
  std::optional<std::uint64_t> current_command;
  // Authored start/end/options remain unproduced; no reader runs here.
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
  std::uint32_t raw_attachment_argument{};
  std::optional<std::uint32_t> fade_start,fade_deadline,fade_state,fade_in_event,fade_out_event;
  std::optional<std::string> target_name;
  std::optional<std::uint32_t> script_reference;
  std::optional<IntroAnimationConstructionState> animation;
  std::optional<IntroCutListConstructionState> cut_list;
  std::optional<std::uint32_t> list_index;
  std::optional<std::int32_t> list_sentinel;
  std::optional<std::vector<std::uint64_t>> commands,auxiliary_list_a,auxiliary_list_b;
  struct ParamAnimationStorage {
    std::array<std::uint32_t,2> local_words{};
    std::uint32_t capacity{32},growth{32},count{},element_control{1};
    std::array<std::uint64_t,2> backing_references{};
  };
  struct ParticleEmitterStorage {
    std::uint64_t local_reference{};
    std::vector<std::uint64_t> handles;
  };
  std::optional<ParamAnimationStorage> param_animation;
  std::optional<ParticleEmitterStorage> particle_emitter;
  struct ScrollTextureState {std::uint16_t start_event{},stop_event{};};
  struct SoundExtendState {
    std::array<float,6> scalars{};
    std::array<std::uint32_t,4> integers{};
    bool option{};
    std::uint32_t category{},output_mode{2};
    std::uint16_t start_event{};
    std::optional<std::uint32_t> option_a,option_b;
    bool phase_one_ordinary_removed{};
  };
  struct SoundNotifyState {
    IntroRuntimeHandle target{};
    std::uint16_t event{};
    std::uint32_t raw_target_reference{},raw_event_reference{};
    std::optional<float> duration_snapshot;
  };
  struct SoundSegmentState {
    std::uint32_t saved_playback{};
    std::array<bool,5> controls{true,true,false,false,true};
    std::array<std::array<std::uint32_t,4>,4> times{};
    float probability{1};
    bool subtitles{};
    std::string subtitle;
    std::array<std::uint16_t,4> events{};
    std::uint32_t raw_start_event_reference{},raw_stop_event_reference{};
    std::optional<std::vector<std::uint64_t>> additional_transient_references;
  };
  struct SoundDefineState {
    bool property_on_parent{};
    std::string property_key;
  };
  struct MovieControlState {
    IntroRuntimeHandle leading_reference{};
    std::uint32_t tracking_word{};
    bool activated{},fixed_destination_override{},outer_option{};
    char destination_prefix{};
    std::array<std::uint16_t,7> events{};
    // Exact cardinality awaits an independently specified field inventory.
    std::optional<std::vector<std::uint32_t>> additional_timing_words;
  };
  std::optional<ScrollTextureState> scroll_texture;
  std::optional<std::string> command_text;
  std::optional<SoundExtendState> sound_extend;
  std::optional<SoundNotifyState> sound_notify;
  std::optional<SoundSegmentState> sound_segment;
  std::optional<SoundDefineState> sound_define;
  std::optional<MovieControlState> movie_control;
};
struct IntroConstructedCharacterOwner {
  IntroConstructedPictureOwner visual;
  std::uint32_t character_control{64},local_counter{};
  bool extra_boolean{},owned_storage_available{};
};
struct IntroConstructedListOwner {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::string name;
  std::uint32_t class_identifier{0x0800001aU},count{},component_mask{};
  bool backing_available{};
  std::vector<std::uint64_t> attachments;
  struct AnimationStorage {
    std::uint32_t capacity{32},growth{32},count{},element_control{1};
    std::array<std::uint64_t,2> backing_references{};
  };
  std::unique_ptr<AnimationStorage> animation_storage;
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
  std::optional<IntroRuntimeHandle> cached_window{},derived_reference{};
  std::vector<IntroRuntimeResourceHandle> category_two{};
  std::shared_ptr<IntroOwnerAuxiliary> auxiliary_state{};
};
struct IntroConstructedRoomOwner {
  IntroAuthoredGroupOwner group;
  bool room_mode{},enabled{};
  std::vector<IntroRuntimeHandle> rooms;
  std::vector<IntroRuntimeResourceHandle> ordinary_members;
};
struct IntroConstructedObjectOwner {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::string name;
  std::uint32_t class_identifier{},flags{};
  bool classification{},backing_available{};
  std::unique_ptr<IntroOwnerAuxiliary> auxiliary;
  std::optional<bool> byte_control;
  std::optional<std::uint32_t> local_control;
  std::optional<IntroRuntimeHandle> local_reference;
  std::optional<std::array<IntroRuntimeHandle,2>> self_links,associated_references;
  std::optional<std::array<float,2>> scalar_pair;
  struct ParticleUsageState {
    bool diagnostic_enabled{};
    std::uint16_t activate_event{};
    std::array<std::byte,2048> controls{};
    std::uint32_t local_control{};
    struct Pool {
      std::array<std::uint32_t,64> available_slots;
      std::uint32_t available_count{64};
      // Payloads are unavailable until an actual particle producer.
      std::array<std::optional<std::vector<std::byte>>,64> payloads;
    };
    std::unique_ptr<Pool> pool;
    struct ConsoleDescriptor {
      std::string key{"particle_usage"};
      bool* target{};
      runtime::LiveVariableLease registration;
    };
    std::unique_ptr<ConsoleDescriptor> descriptor;
  };
  std::unique_ptr<ParticleUsageState> particle_usage;
};
struct IntroSavedResourceFlags {
  IntroRuntimeResourceHandle resource;
  std::uint32_t flags;
};
struct IntroDeferredReaderWork {
  IntroRuntimeResourceHandle resource;
  std::uint32_t source_offset;
  // Directory identity is retained independently of the allocator resource.
  // It is the only domain used for deferred authored-reference translation.
  std::size_t source_directory_index{};
  bool processed{};
  // A null authored word remains null.  Non-null entries are immutable native
  // resource identities resolved before either reader boundary observes work.
  std::vector<std::optional<IntroRuntimeResourceHandle>> translated_references;
};
enum class IntroLifecyclePreflightFailure : std::uint8_t {
  none,stage,reader_coverage,component_coverage,owner_coverage,live_mapping,unsupported
};
// Snapshot of the dynamic scene inventory required before normal startup may
// enter its reader bracket or global lifecycle. Counts are evidence, never a
// substitute for typed implementation registration.
struct IntroLifecyclePreflightReport {
  std::size_t expected_readers{},covered_readers{};
  std::size_t expected_components{},covered_components{};
  std::size_t expected_owners{},covered_owners{};
  IntroLifecyclePreflightFailure failure{IntroLifecyclePreflightFailure::unsupported};
  [[nodiscard]] bool ready() const noexcept { return failure==IntroLifecyclePreflightFailure::none; }
};
enum class IntroDeferredReaderFamily : std::uint8_t {
  unclassified, sound_owner, window_owner, movie_controller,
  first_cut_sequence, first_cut_list, first_cut_legal_picture,
  external_cut_commands, first_cut_fade_picture, first_cut_camera,
};
enum class IntroDeferredReaderImplementationState : std::uint8_t {
  unimplemented, implemented_not_applied, applied,
};
struct IntroDeferredReaderCoverageEntry {
  std::uint32_t source_type{};
  IntroDeferredReaderFamily family{IntroDeferredReaderFamily::unclassified};
  IntroDeferredReaderImplementationState state{
      IntroDeferredReaderImplementationState::unimplemented};
  std::size_t count{};
};
struct IntroDeferredReaderCoverageInventory {
  IntroReaderBracketStage stage{IntroReaderBracketStage::not_started};
  std::size_t total_discovered{}, total_supported{}, total_applied{};
  std::vector<IntroDeferredReaderCoverageEntry> entries;
};
// Immutable source-format observation only. These counters neither admit a
// deferred reader nor describe an executable attachment sequence.
struct IntroMatPosDeferredDispatchInventory {
  std::size_t associated_records{};
  std::size_t terminal_before_first_attachment_delimiter{};
  std::size_t attachment_delimiter_precedes_terminal{};
  std::size_t attachment_delimiters{};
};
// Live values are owned by the real lifecycle caller. This adapter must never
// derive them from archived source flags or prepared picture positions.
struct FirstCutLegalPictureActivationPrerequisites {
  bool global_lifecycle_complete{},positive_time_member_activated{},owner_present{true};
  std::uint32_t& picture_runtime_flags;
  std::optional<std::uint32_t> parent_runtime_flags;
  std::array<float,3>& picture_position;
  std::uint32_t& center_component_status;
  std::int32_t engine_width{},engine_height{};
  std::function<void()> position_update_service;
  std::function<void(cutscene::PictureActivationPrefix::Stage)> activation_stage;
};
struct FirstCutLegalPictureActivationResult {
  std::size_t source{},center_component{};
  bool activation_prefix_complete{};
};
// Published by the concrete MovieControl owner reader only.  Source references
// with an established directory mapping are retained alongside their authored
// values.  This is immutable reader state, not CutSequence construction or
// controller execution.
struct IntroMovieControllerReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t component_index{};
  data::GmsIntroMovieControllerSource authored;
  IntroRuntimeResourceHandle sequence_list_resource;
  IntroRuntimeResourceHandle group_list_resource;
  std::optional<IntroRuntimeResourceHandle> additional_resource;
  std::optional<IntroRuntimeResourceHandle> first_optional_resource;
  std::optional<IntroRuntimeResourceHandle> second_optional_resource;
  std::vector<std::optional<IntroRuntimeResourceHandle>> sequence_members;
  std::vector<std::optional<IntroRuntimeResourceHandle>> group_members;
};
// This follows the reviewed MovieControl owner reader. It retains only
// construction-owned component state; it does not enroll event 16 or create a
// lifecycle admission.
struct IntroMovieControllerComponentReaderState {
  IntroRuntimeHandle owner;
  std::size_t component_index{};
  std::uint16_t class_ordinal{};
  std::uint32_t requested_mask{};
  std::uint32_t priority{};
  std::array<std::uint16_t, 7> events{};
};
// Immutable state published by the two reviewed first-cut list readers.  It
// retains authored data and source-directory resource mappings only; it does
// not construct a CutList, register events, or schedule commands.
struct IntroFirstCutSequenceReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t component_index{};
  data::GmsIntroCutSequenceSource authored;
  std::array<std::optional<IntroRuntimeResourceHandle>,6> members;
};
// Cold provenance for the completed first-cut sequence component boundary.
// It neither creates a player nor establishes timing or lifecycle behavior.
struct IntroFirstCutSequenceComponentReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t source_directory_index{};
  std::uint32_t source_offset{};
  std::size_t component_index{};
  data::GmsIntroCutSequenceSource authored;
  std::array<std::optional<IntroRuntimeResourceHandle>,6> members;
};
struct IntroFirstCutListReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::array<std::size_t,6> component_indices{};
  data::GmsIntroFirstCutSource authored;
  IntroRuntimeResourceHandle sequence_resource;
  std::array<IntroRuntimeResourceHandle,5> command_target_resources;
};
// Immutable component-reader evidence for the reviewed first-cut owner form.
// It preserves bounded payload values and component ownership without granting
// lifecycle/component admission or creating a player.
struct IntroFirstCutComponentReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::array<std::size_t,6> component_indices{};
  data::FirstCutComponentPayloadSession payloads;
};
// A checked raw camera-owner boundary.  It retains the parser-validated source
// only; conversion, renderer registration, selection and frame admission stay
// outside the reader bracket.
struct IntroFirstCutCameraReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  data::GmsIntroCameraSource authored;
};
// Materialized source state for the one reviewed first-cut player. It is not a
// scheduler, event registration, camera route, or renderer admission.
struct IntroFirstCutPlayerPreparedState {
  IntroRuntimeHandle list_owner, sequence_owner;
  IntroRuntimeResourceHandle list_resource, sequence_resource;
  std::size_t list_component_index{}, sequence_component_index{};
  std::array<std::uint32_t,3> leading_controls{};
  std::uint32_t raw_scalar{};
  std::array<std::uint32_t,3> trailing_controls{};
  float list_value{};
  std::array<float,2> sequence_values{};
  std::uint32_t raw_enabled_option{};
  std::array<bool,1> started{}, completed{};
};
// Immutable state for the one reviewed external-command pair. It cannot run a
// command, enroll events, or mutate a CutSequenceList.
struct IntroExternalCutCommandsReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::array<std::size_t,2> component_indices{};
  std::array<data::GmsIntroCutCommandSource,2> commands{};
  std::array<IntroRuntimeResourceHandle,2> external_list_resources{};
};
struct IntroFadePictureReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t component_index{};
  data::GmsWindowPictureSource authored;
  std::uint32_t picture_asset_reference{};
};
// A cold component-boundary receipt for a first-cut FadeToBlack owner.  The
// authored picture stream is owner data; this does not infer fade semantics.
struct IntroFadePictureComponentReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t source_directory_index{};
  std::uint32_t source_offset{};
  std::size_t component_index{};
  data::GmsWindowPictureSource authored;
  std::uint32_t picture_asset_reference{};
};
struct IntroLegalPictureReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t component_index{};
  data::GmsWindowPictureSource authored;
  std::uint32_t picture_asset_reference{};
};
// A cold receipt for the completed Center component boundary. It establishes
// provenance only; position/cache work remains part of later activation.
struct IntroLegalPictureComponentReaderState {
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
  std::size_t source_directory_index{};
  std::uint32_t source_offset{};
  std::size_t component_index{};
  data::GmsWindowPictureSource authored;
  std::uint32_t picture_asset_reference{};
};
struct IntroSourceScriptWork {
  IntroRuntimeResourceHandle resource;
  std::uint32_t source_offset;
};
struct IntroCameraRegistrationServices {
  std::function<std::int32_t()> width,height;
  std::function<bool()> backend_ready;
  std::function<void(IntroRuntimeHandle)> admit_view;
};
// Opaque service boundary recovered from the loader tail.  The retained value
// is deliberately passed through without assigning it a native meaning.
// Reader callbacks are ordering hooks only; parsing and translation belong to
// later approved work.
struct IntroPostconstructionReaderServices {
  std::function<void(std::uint64_t)> external_loader_service;
  std::function<void(const IntroSourceScriptWork&)> source_script_work;
  std::function<void()> pre_reader_service;
  std::function<void(const IntroDeferredReaderWork&)> prepare_deferred_reader;
  std::function<void(const IntroDeferredReaderWork&)> owner_reader_boundary;
  std::function<void(const IntroDeferredReaderWork&)> component_reader_boundary;
  std::function<void()> end_reader_service;
};
// Pre-release loader-tail inputs retain only the boundary shape established by
// the observed route.  They are deliberately not a public GMS grammar.
struct IntroNamedGlobalPayload {
  std::span<const std::byte> bytes;
};
struct IntroRendererResourcePayload {
  std::span<const std::byte> bytes;
};
struct IntroRendererResourceContainer {
  std::uint64_t identity{};
  bool operator==(const IntroRendererResourceContainer&) const = default;
};
struct IntroResourceAssociationRecord {
  // Raw pair values. The ordinary loader adds 0x70 and then applies the GMS
  // 0x40000000 reference-domain marker independently before each lookup.
  std::uint32_t first_reference{},second_reference{};
};
struct IntroAuxiliaryArraySources {
  std::optional<std::vector<std::array<std::byte,12>>> first;
  std::optional<std::vector<std::array<std::byte,8>>> second;
};
// Opaque, concrete-service boundaries in the ordinary loader tail. Callers
// must supply actual readers/parsers/resolvers. The runtime never treats a
// present source section as successfully consumed by a no-op substitute.
struct IntroOuterLoaderTailServices {
  std::optional<IntroNamedGlobalPayload> named_global_payload;
  // The relocation callback receives an owned copy of the complete tagged
  // block (including its header). It is responsible for only independently
  // verified reference relocations; it must not imply typed-reader success.
  std::function<void(IntroNamedGlobalPreparedReader&)> relocate_named_global_references;
  std::function<void(std::string_view,IntroNamedGlobalPreparedReader&)> read_named_global_payload;
  std::optional<IntroRendererResourcePayload> renderer_resource_payload;
  // Resolves the reviewed 32-bit renderer-prefix lookup key. A payload with
  // nonzero tagged references cannot be handed to its typed reader without it.
  std::function<std::optional<std::uint32_t>(std::uint32_t)> resolve_renderer_reference;
  std::function<IntroRendererResourceContainer(std::span<const std::byte>)> parse_renderer_resource_payload;
  std::function<void(IntroRendererResourceContainer)> release_renderer_construction_reference;
  std::vector<IntroResourceAssociationRecord> resource_associations;
  std::function<void(IntroRuntimeResourceHandle,IntroRuntimeResourceHandle)> associate_live_resources;
  IntroAuxiliaryArraySources auxiliary_arrays;
  std::function<void()> release_loader_source_lease;
  std::function<bool()> camera_zero_present;
  bool single_allocation_mode{};
  std::function<void(IntroRuntimeResourceHandle)> enqueue_transform;
  IntroCameraRegistrationServices fallback_camera_registration;
  std::function<void()> outer_scene_operation;
  std::function<void()> between_saved_scene_operation;
  std::function<void()> intermediate_scene_finalization;
  std::function<void(IntroRuntimeResourceHandle,bool)> spatial_admission;
  std::function<void(IntroRuntimeResourceHandle,bool)> saved_0x4000_service;
};
struct IntroWindowOwner {
  IntroAuthoredGroupOwner group;
  IntroRuntimeHandle enclosing_window{},selected_camera{},cursor{},auxiliary{};
  // The reviewed Window reader retains these authored 0x88 references as
  // source-directory resource mappings. They are not interpreted as cursors,
  // owner links, or activation work.
  std::array<std::optional<IntroRuntimeResourceHandle>,2> opaque_reference_resources{};
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
  std::optional<std::uint64_t> object_token{};
  std::optional<IntroRuntimeHandle> owner_handle{};
  std::uint32_t setter_flags{};
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
  std::vector<IntroRuntimeHandle> rooms{};
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
  [[nodiscard]] bool has_record() const noexcept {return lease_.binding()!=0;}
  [[nodiscard]] bool source_applied() const noexcept { return source_applied_; }
private:
  friend class IntroRuntime;
  const IntroPreparedSound* source_{};
  IntroRuntimeHandle handle_;
  audio::SoundRecordLease lease_;
  std::uint64_t owner_binding_{};
  bool active_{}, failed_{}, source_applied_{};
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

// Explicit diagnostic/integration result for the two recovered intro sound
// owners. This is deliberately not a global component lifecycle result.
struct IntroSoundFamilyPhaseOneResult {
  struct Owner {
    std::size_t source{};
    std::size_t extend_component{},notify_component{},segment_component{},define_component{};
    float notify_duration{};
    bool segment_restore{},extend_ordinary_removed{};
    std::string defined_key;
  };
  std::array<Owner,2> owners{};
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
               runtime::SceneComponentSequence& component_sequence,
               std::string selected_scene_filename="FF-Intro.gms",
               IntroSoundLoadPolicy sound_policy=IntroSoundLoadPolicy::prepared_compatibility);
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
  void construct_authored_camera_without_engine_renderer();
  void construct_second_window_picture_without_engine_renderer();
  void construct_second_window_scope_without_engine_renderer();
  void construct_following_visual_scope_without_engine_renderer();
  void construct_room_animation_scope_without_engine_renderer();
  void construct_lens_flare_animation_scope_without_engine_renderer();
  void construct_remaining_directory_without_engine_renderer();
  void set_restore_mode(bool value);
  [[nodiscard]] bool restore_mode() const noexcept {return restore_mode_;}
  [[nodiscard]] IntroSoundLoadPolicy sound_load_policy() const noexcept {return sound_load_policy_;}
  // Runs only the evidenced tail bracket. Restore deliberately does not fall
  // through to ordinary scripts/readers; its later route remains unrecovered.
  void run_postconstruction_reader_bracket(std::uint64_t retained_saved_value,
                                           const IntroPostconstructionReaderServices& services);
  [[nodiscard]] IntroReaderBracketStage reader_bracket_stage() const noexcept {return reader_bracket_stage_;}
  [[nodiscard]] std::optional<std::uint64_t> reader_bracket_retained_saved_value() const noexcept {
    return reader_bracket_retained_saved_value_;
  }
  // Ordinary-only outer tail through the two saved-resource service passes.
  // Global lifecycle, rendering, audio, and later scene operations remain
  // outside this boundary. Missing concrete services fail visibly.
  void run_outer_loader_tail_through_saved_services(const IntroOuterLoaderTailServices& services);
  [[nodiscard]] IntroOuterLoaderTailStage outer_loader_tail_stage() const noexcept {return outer_loader_tail_stage_;}
  [[nodiscard]] bool loader_source_lease_released() const noexcept {return loader_source_lease_released_;}
  [[nodiscard]] std::optional<IntroRendererResourceContainer> renderer_resource_container() const noexcept {
    return renderer_resource_container_;
  }
  [[nodiscard]] std::optional<IntroRuntimeResourceHandle>
  resolve_marked_source_resource_reference(std::uint32_t reference) const;
  [[nodiscard]] std::span<const std::array<std::byte,12>> first_auxiliary_array() const noexcept {
    return first_auxiliary_array_;
  }
  [[nodiscard]] std::span<const std::array<std::byte,8>> second_auxiliary_array() const noexcept {
    return second_auxiliary_array_;
  }
  [[nodiscard]] const IntroConstructedRoomOwner* constructed_room_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedObjectOwner* constructed_object_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroOwnerAuxiliary* constructed_owner_auxiliary(std::size_t source) const noexcept;
  // Builds the immutable, owner-local KEYS resolver from the completed
  // directory's retained MatPosAnim property blocks. This is preparation only:
  // it neither reads a component source nor runs a lifecycle phase.
  void prepare_scene_lifetime_keys_registry();
  [[nodiscard]] const data::SceneLifetimeKeysRegistry* scene_lifetime_keys_registry() const noexcept {
    return scene_lifetime_keys_registry_ ? &*scene_lifetime_keys_registry_ : nullptr;
  }
  // Freezes the complete retained FF-Intro BUF allocation for the already
  // prepared owner-local KEYS registry. Every registered MatPos descriptor
  // must bind and be readable at both ends of its declared range before this
  // view is published. This is read-only preparation, not phase one.
  void prepare_scene_lifetime_keys_backing();
  [[nodiscard]] const data::ImmutableKeysBackingView* scene_lifetime_keys_backing() const noexcept {
    return scene_lifetime_keys_backing_ ? &*scene_lifetime_keys_backing_ : nullptr;
  }
  [[nodiscard]] std::optional<data::KeysBackingSample> evaluate_scene_lifetime_keys(
      IntroRuntimeHandle owner, float normalized_coordinate) const noexcept;
  [[nodiscard]] std::span<const IntroSavedResourceFlags> saved_resource_flags() const noexcept {return saved_resource_flags_;}
  [[nodiscard]] bool light_policy() const noexcept {return light_policy_;}
  void set_light_policy(bool value) noexcept {light_policy_=value;}
  void set_scene_object_property_native(std::string key,std::uint64_t token);
  void set_scene_owner_property_native(std::string key,IntroRuntimeHandle owner,std::uint32_t flags);
  void assign_owner_property_data(std::size_t source,std::span<const std::byte> section);
  [[nodiscard]] const IntroConstructedPictureOwner* constructed_visual_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroAuthoredGroupOwner* constructed_group_owner(std::size_t source) const noexcept;
  [[nodiscard]] PositionServiceMode directory_position_mode() const noexcept {return position_mode_;}
  [[nodiscard]] const PositionUpdateService& position_updates() const noexcept {return position_updates_;}
  [[nodiscard]] const IntroConstructedCharacterOwner* constructed_character_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedListOwner* constructed_list_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedPictureComponent* constructed_attachment(std::size_t component) const noexcept;
  [[nodiscard]] const IntroConstructedCameraOwner* constructed_camera_owner() const noexcept {
    return constructed_camera_owner(resources_.camera_index());
  }
  [[nodiscard]] const IntroConstructedCameraOwner* constructed_camera_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedPictureOwner* constructed_picture_owner(std::size_t source) const noexcept;
  [[nodiscard]] const IntroConstructedPictureComponent* constructed_picture_component(std::size_t source) const noexcept;
  [[nodiscard]] const IntroWindowOwner* window_owner() const noexcept {return window_owner_;}
  [[nodiscard]] IntroWindowOwner& window_for_owner(IntroRuntimeHandle owner);
  [[nodiscard]] const IntroWindowOwner& window_for_owner(IntroRuntimeHandle owner) const;
  // One reviewed ZWINDOWS owner-reader form only. The caller explicitly opts
  // into it from owner_reader_boundary after deferred preparation; this does
  // not make the surrounding reader bracket or normal startup complete.
  void apply_supported_window_deferred_reader(const IntroDeferredReaderWork& work);
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
  // Does not execute a reader, lifecycle callback or host service. Current
  // concrete coverage is intentionally incomplete, so ordinary startup must
  // remain outside this boundary until typed registrations are installed.
  [[nodiscard]] IntroLifecyclePreflightReport preflight_global_lifecycle() const;
  // Aggregate-only recovery inventory. It exposes neither source identities nor
  // payloads, and classifies a family only through an existing reviewed reader
  // predicate; equal source types are not treated as interchangeable.
  [[nodiscard]] IntroDeferredReaderCoverageInventory reader_coverage_inventory() const;
  // Read-only structural audit of authored MatPosAnim deferred blocks. It is
  // deliberately separate from deferred_reader_work_ and lifecycle admission.
  [[nodiscard]] IntroMatPosDeferredDispatchInventory matpos_deferred_dispatch_inventory() const;
  // Explicit first-cut activation bridge. It is disconnected from normal
  // startup and does not create a view, submit a draw or dispatch cut events.
  [[nodiscard]] FirstCutLegalPictureActivationResult activate_first_cut_legal_picture(
      const FirstCutLegalPictureActivationPrerequisites& prerequisites);
  [[nodiscard]] std::span<const IntroSourceScriptWork> source_script_work() const noexcept {return source_script_work_;}
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
  // Applies one parsed sound-owner prefix at its matching owner-reader
  // boundary. This does not prepare playback, register an attachment event or
  // admit a global lifecycle phase.
  void apply_supported_sound_owner_deferred_reader(const IntroDeferredReaderWork& work);
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
  // Runs only the recovered first-phase sound-family probe. It requires both
  // real owner readers and pre-hooks to have completed, never invokes generic
  // component callbacks, and is intentionally unavailable to normal startup.
  const IntroSoundFamilyPhaseOneResult& run_isolated_sound_family_phase_one();
  [[nodiscard]] const IntroSoundFamilyPhaseOneResult* isolated_sound_family_phase_one() const noexcept {
    return isolated_sound_family_phase_one_?&*isolated_sound_family_phase_one_:nullptr;
  }
  // Catalog membership includes unconstructed/removed entries, not a live
  // owner attachment collection suitable for runtime lookup or disposal.
  [[nodiscard]] std::span<const std::size_t> owner_components(IntroRuntimeHandle owner) const;
  // Component 0 describes the synthesized RootGroup; all authored attachments
  // follow in directory/attachment order. Catalog order is not construction.
  [[nodiscard]] std::size_t controller_component_index() const noexcept { return controller_component_; }
  // Complete only the checked MovieControl owner-reader boundary.  This does
  // not run component phases, enroll events, or activate the controller.
  void apply_supported_movie_control_deferred_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const IntroMovieControllerReaderState* movie_controller_reader_state() const noexcept {return movie_controller_reader_state_?&*movie_controller_reader_state_:nullptr;}
  void apply_supported_movie_control_component_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const IntroMovieControllerComponentReaderState* movie_controller_component_reader_state() const noexcept {
    return movie_controller_component_reader_state_?&*movie_controller_component_reader_state_:nullptr;
  }
  // Complete only the two checked first-cut list reader boundaries. These
  // publish immutable source/resource state and do not execute cutscene work.
  void apply_supported_first_cut_sequence_deferred_reader(const IntroDeferredReaderWork& work);
  void apply_supported_first_cut_sequence_component_reader(const IntroDeferredReaderWork& work);
  void apply_supported_first_cut_list_deferred_reader(const IntroDeferredReaderWork& work);
  // Reads the six bounded first-cut attachment payloads after their owner
  // reader. This retains parsed source values only; it cannot schedule a cut
  // or cause a lifecycle, audio, or renderer effect.
  void apply_supported_first_cut_component_reader(const IntroDeferredReaderWork& work);
  void apply_supported_first_cut_camera_deferred_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const IntroFirstCutSequenceReaderState* first_cut_sequence_reader_state() const noexcept {return first_cut_sequence_reader_state_?&*first_cut_sequence_reader_state_:nullptr;}
  [[nodiscard]] const IntroFirstCutSequenceComponentReaderState* first_cut_sequence_component_reader_state() const noexcept {return first_cut_sequence_component_reader_state_?&*first_cut_sequence_component_reader_state_:nullptr;}
  [[nodiscard]] const IntroFirstCutListReaderState* first_cut_list_reader_state() const noexcept {return first_cut_list_reader_state_?&*first_cut_list_reader_state_:nullptr;}
  [[nodiscard]] const IntroFirstCutComponentReaderState* first_cut_component_reader_state() const noexcept {return first_cut_component_reader_state_?&*first_cut_component_reader_state_:nullptr;}
  [[nodiscard]] const IntroFirstCutCameraReaderState* first_cut_camera_reader_state() const noexcept {return first_cut_camera_reader_state_?&*first_cut_camera_reader_state_:nullptr;}
  // Atomically materialize the source-backed first-cut player state after both
  // reviewed readers. Playback services remain intentionally absent.
  void prepare_supported_first_cut_player();
  [[nodiscard]] const IntroFirstCutPlayerPreparedState* first_cut_player_prepared_state() const noexcept {
    return first_cut_player_prepared_state_?&*first_cut_player_prepared_state_:nullptr;
  }
  // Projects the two source-validated first-cut reader states into the
  // dedicated lifecycle boundary. It does not run either phase.
  [[nodiscard]] cutscene::FirstCutPlayerInitialization first_cut_player_initialization() const;
  // Projects the same source-validated state into the composed lifecycle
  // session. It remains cold and does not admit global lifecycle work.
  [[nodiscard]] cutscene::FirstCutPlayerSession first_cut_player_session() const;
  void apply_supported_external_cut_commands_deferred_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const IntroExternalCutCommandsReaderState* external_cut_commands_reader_state() const noexcept {return external_cut_commands_reader_state_?&*external_cut_commands_reader_state_:nullptr;}
  void apply_supported_first_cut_fade_picture_deferred_reader(const IntroDeferredReaderWork& work);
  void apply_supported_first_cut_fade_picture_component_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const std::map<std::size_t,IntroFadePictureReaderState>& fade_picture_reader_states() const noexcept {return fade_picture_reader_states_;}
  [[nodiscard]] const std::map<std::size_t,IntroFadePictureComponentReaderState>& fade_picture_component_reader_states() const noexcept {return fade_picture_component_reader_states_;}
  void apply_supported_first_cut_legal_picture_deferred_reader(const IntroDeferredReaderWork& work);
  void apply_supported_first_cut_legal_picture_component_reader(const IntroDeferredReaderWork& work);
  [[nodiscard]] const IntroLegalPictureReaderState* legal_picture_reader_state() const noexcept {return legal_picture_reader_state_?&*legal_picture_reader_state_:nullptr;}
  [[nodiscard]] const IntroLegalPictureComponentReaderState* legal_picture_component_reader_state() const noexcept {return legal_picture_component_reader_state_?&*legal_picture_component_reader_state_:nullptr;}
  // Caller still owes actual global lifecycle admission and external services.
  // Clock/audio resolve through the same application state retained by this scene.
  void run_controller_phase_two(const IntroControllerPhaseTwoServices& external);
  // Install on the concrete MovieControl instance constructed in components().
  [[nodiscard]] runtime::ComponentCallback controller_phase_two_callback(
      const IntroControllerPhaseTwoServices& external);
  [[nodiscard]] IntroControllerInitialization& controller_initialization() noexcept { return controller_initialization_; }
  [[nodiscard]] const IntroControllerInitialization& controller_initialization() const noexcept { return controller_initialization_; }
  [[nodiscard]] FreshIntroCamera& camera();
  [[nodiscard]] const FreshIntroCamera& camera() const;
  // Explicit real renderer membership for the retained authored camera. Does
  // not activate a cut, fabricate backend readiness or synthesize DefaultCam.
  void register_camera(float key,const IntroCameraRegistrationServices& services);
  void register_camera(IntroRuntimeHandle owner,float key,const IntroCameraRegistrationServices& services);
  [[nodiscard]] FreshIntroCamera& camera_for_owner(IntroRuntimeHandle owner);
  [[nodiscard]] const FreshIntroCamera& camera_for_owner(IntroRuntimeHandle owner) const;
  [[nodiscard]] RendererCameraRegistry& registered_cameras() noexcept {return registered_cameras_;}
  [[nodiscard]] IntroRuntimeHandle camera_context() const;
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
  // Read-only source lookup for renderer-neutral consumers.  It neither
  // constructs a picture owner nor changes picture submission state.
  [[nodiscard]] const IntroRuntimePicture& picture_for_source(std::size_t source) const;
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
  [[nodiscard]] cutscene::FirstCutPlayerDescriptor first_cut_player_descriptor() const;
  runtime::ApplicationServices& application_;
  IntroSoundLoadPolicy sound_load_policy_;
  bool restore_mode_{};
  std::uint64_t owner_base_{};
  IntroPreparedResources resources_;
  // Declared before components so their captures are destroyed before leases.
  std::vector<std::unique_ptr<IntroRuntimeSound>> sounds_;
  std::optional<IntroSoundFamilyPhaseOneResult> isolated_sound_family_phase_one_;
  bool isolated_sound_family_phase_one_busy_{},isolated_sound_family_phase_one_failed_{};
  std::shared_ptr<RootGroupComponent> root_group_;
  runtime::ComponentLifecycle components_;
  std::unique_ptr<runtime::OrdinaryComponentManager> ordinary_;
  std::vector<std::vector<std::size_t>> owner_components_;
  std::size_t controller_component_{};
  std::optional<IntroMovieControllerReaderState> movie_controller_reader_state_;
  std::optional<IntroMovieControllerComponentReaderState> movie_controller_component_reader_state_;
  std::optional<IntroFirstCutSequenceReaderState> first_cut_sequence_reader_state_;
  std::optional<IntroFirstCutSequenceComponentReaderState> first_cut_sequence_component_reader_state_;
  std::optional<IntroFirstCutListReaderState> first_cut_list_reader_state_;
  std::optional<IntroFirstCutComponentReaderState> first_cut_component_reader_state_;
  std::optional<IntroFirstCutCameraReaderState> first_cut_camera_reader_state_;
  std::optional<IntroFirstCutPlayerPreparedState> first_cut_player_prepared_state_;
  std::optional<IntroExternalCutCommandsReaderState> external_cut_commands_reader_state_;
  std::map<std::size_t,IntroFadePictureReaderState> fade_picture_reader_states_;
  std::map<std::size_t,IntroFadePictureComponentReaderState> fade_picture_component_reader_states_;
  std::optional<IntroLegalPictureReaderState> legal_picture_reader_state_;
  std::optional<IntroLegalPictureComponentReaderState> legal_picture_component_reader_state_;
  IntroControllerInitialization controller_initialization_;
  FreshIntroCamera prepared_camera_;
  std::map<std::size_t,std::unique_ptr<IntroLiveCameraOwner>> live_cameras_;
  RendererCameraRegistry registered_cameras_;
  IntroRuntimeHandle prepared_camera_context_; // Prepared compatibility only.
  std::vector<PictureHierarchyNode> hierarchy_;
  std::vector<IntroRuntimeHandle> hierarchy_owners_;
  std::map<std::uint64_t,std::uint32_t> owner_indices_;
  std::map<std::uint64_t,std::uint32_t> resource_indices_;
  std::vector<std::optional<IntroRuntimeResourceHandle>> hierarchy_resources_;
  std::vector<std::optional<IntroRuntimeHandle>> resource_owners_;
  std::vector<std::optional<IntroRuntimeResourceState>> resource_states_;
  IntroResourceLoadStage resource_load_stage_{IntroResourceLoadStage::prepared};
  IntroReaderBracketStage reader_bracket_stage_{IntroReaderBracketStage::not_started};
  std::optional<std::uint64_t> reader_bracket_retained_saved_value_;
  IntroOuterLoaderTailStage outer_loader_tail_stage_{IntroOuterLoaderTailStage::not_started};
  bool loader_source_lease_released_{};
  std::optional<IntroRendererResourceContainer> renderer_resource_container_;
  std::vector<std::array<std::byte,12>> first_auxiliary_array_;
  std::vector<std::array<std::byte,8>> second_auxiliary_array_;
  runtime::SceneEventNames event_names_;
  std::vector<std::optional<std::uint32_t>> source_event_name_mapping_;
  bool source_event_names_prepared_{};
  std::optional<float> loading_progress_;
  // Written only after a reviewed typed reader commits its owned state. These
  // identities are preflight evidence, never synthesized lifecycle callbacks.
  std::vector<IntroReaderAdmissionIdentity> supported_reader_admissions_;
  // Written only after the reviewed sound-owner pre-hook completes. This is
  // evidence for that one owner boundary, not a substitute for traversal.
  std::vector<IntroOwnerAdmissionIdentity> supported_owner_admissions_;
  // Written only by the four reviewed sound-family phase-one callbacks after
  // each callback has completed its concrete live-state work.
  std::vector<IntroComponentAdmissionIdentity> supported_component_admissions_;
  void record_supported_reader_admission(const IntroDeferredReaderWork& work);
  void record_supported_owner_admission(std::size_t source, IntroRuntimeHandle owner);
  void record_supported_component_admission(std::size_t component);
  void allocate_source_scope(std::uint32_t count_group);
  std::map<std::size_t,std::unique_ptr<IntroWindowOwner>> window_owners_;
  IntroWindowOwner* window_owner_{}; // Non-owning first-cut convenience, never latest Window.
  std::optional<IntroAuthoredGroupOwner> language_owner_;
  std::map<std::size_t,IntroConstructedPictureOwner> constructed_picture_owners_;
  std::map<std::size_t,IntroConstructedPictureOwner> constructed_visual_owners_;
  std::map<std::size_t,IntroAuthoredGroupOwner> constructed_group_owners_;
  std::map<std::size_t,IntroConstructedRoomOwner> constructed_room_owners_;
  std::map<std::size_t,IntroConstructedObjectOwner> constructed_object_owners_;
  std::vector<IntroSavedResourceFlags> saved_resource_flags_;
  bool light_policy_{};
  std::map<std::size_t,IntroConstructedCharacterOwner> constructed_character_owners_;
  std::map<std::size_t,IntroConstructedListOwner> constructed_list_owners_;
  std::map<std::size_t,IntroConstructedPictureComponent> constructed_picture_components_;
  std::optional<data::SceneLifetimeKeysRegistry> scene_lifetime_keys_registry_;
  std::optional<data::ImmutableKeysBackingView> scene_lifetime_keys_backing_;
  struct SceneLifetimeKeysBackingBinding final {
    std::uint64_t owner{};
    std::uint64_t opaque_handle{};
    data::BoundKeysDescriptorRange descriptor{};
  };
  std::vector<SceneLifetimeKeysBackingBinding> scene_lifetime_keys_backing_bindings_;
  // Dedicated native preparation identity stream. Values are opaque registry
  // identities, never source addresses, BUF offsets, or attachment ordinals.
  std::uint64_t next_scene_lifetime_keys_handle_{1};
  std::uint64_t next_scene_lifetime_keys_backing_generation_{1};
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
  void construct_group_row_without_engine_renderer(std::size_t row);
  void construct_picture_row_without_engine_renderer(std::size_t row);
  void construct_non_group_row_without_engine_renderer(std::size_t row);
  void construct_owner_attachments(std::size_t row,std::uint32_t& mask,std::vector<std::uint64_t>& attachments);
  void apply_directory_transform(std::size_t row,IntroRuntimeResourceHandle resource);
  void assign_fresh_directory_metadata(IntroRuntimeResourceHandle resource,std::uint32_t metadata);
  void assign_directory_property(std::size_t source);
  void attach_directory_owner(std::size_t source,IntroRuntimeResourceHandle resource,IntroRuntimeHandle parent);
  void prepare_deferred_references(IntroDeferredReaderWork& work);
  [[nodiscard]] IntroAuthoredGroupOwner* group_owner(IntroRuntimeHandle owner);
  [[nodiscard]] IntroConstructedRoomOwner* nearest_authored_room(IntroRuntimeHandle parent);
  [[nodiscard]] IntroOwnerAuxiliary& ensure_owner_auxiliary(std::size_t source);
  PositionUpdateService position_updates_;
  std::string selected_scene_filename_;
  PositionServiceMode position_mode_{false,false,0};
  bool directory_position_controls_prepared_{};
  std::vector<IntroRuntimeResourceHandle> loaded_resource_handles_;
  std::vector<std::optional<IntroRuntimeResourceHandle>> directory_resource_mapping_;
  std::vector<IntroDeferredReaderWork> deferred_reader_work_;
  std::vector<IntroSourceScriptWork> source_script_work_;
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
