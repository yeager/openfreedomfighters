#include "off/graphics/intro_runtime.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
constexpr std::array<float,9> engine_identity{0,0,1,0,1,0,1,0,0};
bool engine_identity_bits(const std::array<float,9>& matrix) {
  for(std::size_t i=0;i<engine_identity.size();++i)
    if(std::bit_cast<std::uint32_t>(matrix[i])!=std::bit_cast<std::uint32_t>(engine_identity[i])) return false;
  return true;
}
}

audio::SoundRecord& IntroRuntimeSound::record() {
  if (failed_) throw std::runtime_error("Intro sound owner is failed");
  return lease_.get();
}
const audio::SoundRecord& IntroRuntimeSound::record() const {
  if (failed_) throw std::runtime_error("Intro sound owner is failed");
  return lease_.get();
}

std::optional<std::uint32_t> IntroRuntimePicture::runtime_resource_flags() const {
  if (!runtime_) return std::nullopt;
  const auto& state = runtime_->resource_state(handle_);
  return state ? std::optional{state->flags} : std::nullopt;
}

IntroRuntime::IntroRuntime(IntroPreparedResources&& resources, runtime::ApplicationServices& application,
                           runtime::SceneComponentSequence& component_sequence,std::string selected_scene_filename,
                           IntroSoundLoadPolicy sound_policy)
    : application_(application), sound_load_policy_(sound_policy), resources_(std::move(resources)), components_(component_sequence),
      prepared_camera_(resources_.camera()),selected_scene_filename_(std::move(selected_scene_filename)) {
  if(selected_scene_filename_.empty() || selected_scene_filename_.find('\0')!=std::string::npos)
    throw std::runtime_error("Scene filename must be nonempty and NUL-free");
  auto suffix=selected_scene_filename_.size()>=4?selected_scene_filename_.substr(selected_scene_filename_.size()-4):std::string{};
  for(auto& value:suffix) if(value>='A' && value<='Z') value=static_cast<char>(value-'A'+'a');
  position_mode_.immediate=suffix==".wld" || suffix==".wl2";
  // Concrete scene construction produces suppression zero. Queue collection
  // provenance is established separately by the ordinary loader below.
  const auto& directory = resources_.sources().directory();
  if (directory.size() >= std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("intro hierarchy exceeds native index capacity");
  owner_base_=application_.allocate_runtime_owners(directory.size()+1);
  prepared_camera_context_=root_handle();
  owner_components_.resize(directory.size()+1);
  owner_components_[0].push_back(components_.append({root_handle().value, std::nullopt,
      std::nullopt, "ZGROUP_RootGroup", 0, 0, 0.0F, true}));
  for (std::size_t index=0; index<directory.size(); ++index) {
    const auto& source = directory[index];
    for (std::size_t attachment=0; attachment<source.attachments.size(); ++attachment) {
      const auto& input = source.attachments[attachment];
      const auto entry = components_.append({source_handle(index).value, index, attachment,
          std::string(resources_.sources().attachment_identifier(index,attachment)),
          input.source_offset, source.deferred_source_offset, input.parameter, false});
      owner_components_[index+1].push_back(entry);
      if (index == resources_.controller_index()) controller_component_ = entry;
    }
  }
  hierarchy_.reserve(directory.size()+1);
  additional_.reserve(directory.size());
  hierarchy_.push_back({engine_identity, {0.0F,0.0F,0.0F}, no_picture_transform_parent});
  std::uint32_t current = 0;
  for (std::size_t index=0; index<directory.size(); ++index) {
    const auto& source = directory[index];
    for (unsigned step=0; step<source.parent_steps; ++step) {
      if (current == 0) throw std::runtime_error("intro group ascent escapes scene root");
      current = hierarchy_.at(current).parent;
    }
    hierarchy_.push_back({source.basis,source.position,current});
    additional_.push_back(source_handle(index));
    if (source.enters_child_pool) current = static_cast<std::uint32_t>(index+1);
  }
  hierarchy_owners_.reserve(hierarchy_.size());
  resource_states_.resize(hierarchy_.size());
  hierarchy_resources_.reserve(hierarchy_.size());
  resource_owners_.reserve(hierarchy_.size());
  for(std::size_t index=0;index<hierarchy_.size();++index) {
    const IntroRuntimeHandle owner{owner_base_+index};
    hierarchy_owners_.push_back(owner);
    owner_indices_.emplace(owner.value,static_cast<std::uint32_t>(index));
    resource_indices_.emplace(owner.value,static_cast<std::uint32_t>(index));
    hierarchy_resources_.push_back(IntroRuntimeResourceHandle{owner.value});
    resource_owners_.push_back(owner);
  }

  // Validate all alias ranges before installing any borrowed mutable storage.
  for (const auto& picture : resources_.pictures()) {
    const auto key = picture.source.picture_asset_reference;
    const std::uint64_t begin = static_cast<std::uint64_t>(key)+4;
    const std::uint64_t end = begin+40*picture.picture.descriptors().size();
    for (const auto& other : resources_.pictures()) {
      const auto other_key = other.source.picture_asset_reference;
      if (key == other_key) continue;
      const std::uint64_t other_begin = static_cast<std::uint64_t>(other_key)+4;
      const std::uint64_t other_end = other_begin+40*other.picture.descriptors().size();
      if (begin != end && other_begin != other_end && begin < other_end && other_begin < end)
        throw std::runtime_error("intro descriptor ranges overlap at different PRM identities");
    }
    auto [entry, inserted] = descriptors_.try_emplace(key,
        picture.picture.descriptors().begin(),picture.picture.descriptors().end());
    if (!inserted && entry->second.size() != picture.picture.descriptors().size())
      throw std::runtime_error("intro shared descriptor identity has inconsistent size");
    for (const auto& binding : picture.bindings.entries()) materials_.try_emplace(binding.prm_offset,0U);
  }
  std::vector<const IntroPreparedPicture*> ordered;
  for (const auto& picture : resources_.pictures()) ordered.push_back(&picture);
  std::ranges::sort(ordered, {}, &IntroPreparedPicture::directory_index);
  pictures_.reserve(ordered.size());
  for (const auto* source : ordered) {
    auto picture = std::make_unique<IntroRuntimePicture>();
    picture->source_ = source;
    picture->runtime_ = this;
    picture->handle_ = source_handle(source->directory_index);
    const auto& record = directory.at(source->directory_index);
    picture->renderer_resource_id_ = record.class_data_value;
    picture->source_flags_ = record.object_flags;
    picture->descriptors_ = &descriptors_.at(source->source.picture_asset_reference);
    std::vector<std::uint32_t*> paired;
    for (const auto& binding : source->bindings.entries()) paired.push_back(&materials_.at(binding.prm_offset));
    picture->colors_ = std::make_unique<PictureColorState>(source->source.authored_alpha,
        0xffffffffU,0U,*picture->descriptors_,paired);
    picture->cache_.invalidate();
    picture->colors_->refresh_material(source->source.base_render_property);
    pictures_.push_back(std::move(picture));
  }
  // Prepared-only compatibility explicitly retains the existing standalone
  // sound reader adapter. Directory construction keeps this source catalog
  // separate: its concrete sound factories allocate records at their rows.
  sounds_.reserve(resources_.sounds().size());
  for (const auto& source : resources_.sounds()) {
    auto owner = std::make_unique<IntroRuntimeSound>();
    owner->source_ = &source;
    owner->handle_ = source_handle(source.directory_index);
    if(sound_load_policy_==IntroSoundLoadPolicy::prepared_compatibility) {
      owner->lease_ = application_.sound_records().create(owner->handle_.value);
      application_.sound_records().apply_source(owner->lease_.get(),source.source);
    }
    sounds_.push_back(std::move(owner));
  }
}

void IntroRuntime::set_restore_mode(bool value) {
  if(resource_load_stage_!=IntroResourceLoadStage::prepared)
    throw std::runtime_error("Restore mode must be supplied before source construction");
  restore_mode_=value;
}

IntroRuntimeSound& IntroRuntime::sound_for_source(std::size_t source) {
  for (auto& sound : sounds_) if (sound->source_index() == source) return *sound;
  throw std::runtime_error("Intro source has no retained sound owner");
}

void IntroRuntime::apply_supported_sound_owner_deferred_reader(
    const IntroDeferredReaderWork& work) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      !work.processed)
    throw std::runtime_error("Sound owner reader requires processed directory work");
  const auto& directory=resources_.sources().directory();
  if(work.source_directory_index>=directory.size())
    throw std::runtime_error("Sound owner reader source is out of range");
  const auto& owner_source=directory[work.source_directory_index];
  if(owner_source.source_type!=0x00200012U || !owner_source.deferred_source_offset ||
      owner_source.deferred_source_offset!=work.source_offset ||
      directory_resource_mapping_.at(work.source_directory_index)!=work.resource ||
      !associated_resource_owner(work.resource) ||
      associated_resource_owner(work.resource)!=source_handle(work.source_directory_index))
    throw std::runtime_error("Deferred work is not a supported sound owner form");

  auto& sound=sound_for_source(work.source_directory_index);
  if(sound.handle()!=source_handle(work.source_directory_index) || sound.source_applied_ ||
      sound.owner_binding_ || sound.active_)
    throw std::runtime_error("Sound owner source reader cannot run twice or after preparation");

  // A scene can retain its parsed sound graph without an output registry. That
  // path still retains every component field below, but owns no canonical
  // record to mutate and therefore cannot accidentally issue playback.
  if(sound.has_record())
    application_.sound_records().apply_source(sound.record(),sound.source_->source);

  const auto attachments=owner_components(sound.handle());
  constexpr std::array<std::string_view,4> factories{
      "ZSNDOBJ_SoundExtend","ZSNDOBJ_SoundNotify","ZSNDOBJ_SoundSegment","ZGEOM_ZSetZDefine"};
  if(attachments.size()!=factories.size())
    throw std::runtime_error("Sound owner reader attachment count is unsupported");
  for(std::size_t index=0;index<attachments.size();++index)
    if(components_.at(attachments[index]).source().factory_name!=factories[index])
      throw std::runtime_error("Sound owner reader attachment order is unsupported");

  const auto& authored=sound.source_->attachments;
  auto& extend=constructed_picture_components_.at(attachments[0]).sound_extend;
  auto& notify=constructed_picture_components_.at(attachments[1]).sound_notify;
  auto& segment=constructed_picture_components_.at(attachments[2]).sound_segment;
  auto& define=constructed_picture_components_.at(attachments[3]).sound_define;
  if(!extend || !notify || !segment || !define)
    throw std::runtime_error("Sound owner reader components are not constructed");
  extend->scalars=authored.extend.scalars;
  extend->integers=authored.extend.integers;
  extend->option=authored.extend.option;
  extend->category=authored.extend.category;
  extend->option_a=authored.extend.option_a;
  extend->option_b=authored.extend.option_b;
  extend->output_mode=authored.extend.authored_output_mode;
  notify->raw_target_reference=authored.notify.target_reference;
  notify->raw_event_reference=authored.notify.event_reference;
  segment->controls[0]=authored.segment.enabled;
  segment->times=authored.segment.times;
  segment->probability=authored.segment.probability;
  segment->subtitles=authored.segment.subtitles;
  segment->subtitle=authored.segment.subtitle;
  segment->raw_start_event_reference=authored.segment.start_event_reference;
  segment->raw_stop_event_reference=authored.segment.stop_event_reference;
  define->property_on_parent=authored.property_on_parent;
  define->property_key=authored.property_key;
  sound.source_applied_=true;
  record_supported_reader_admission(work);
}

void IntroRuntime::construct_root() {
  if(root_owner_state_ && root_owner_state_->enabled && resource_load_stage_!=IntroResourceLoadStage::failed) return;
  if(resource_load_stage_!=IntroResourceLoadStage::prepared)
    throw std::runtime_error("ROOT construction is active or previously failed");
  if(resource_allocation_enabled_ || components_.construction_mode() || components_.failed() ||
      !components_.construction_order().empty() || default_camera_ || ordinary_ || !registered_cameras_.entries().empty() ||
      std::ranges::any_of(resource_states_,[](const auto& state){return state.has_value();}))
    throw std::runtime_error("Fresh ROOT requires inactive allocation before resource/component construction");
  root_attachments_.reserve(1);
  IntroRootOwnerState metadata{"ROOT",0x00100021U};
  resource_load_stage_=IntroResourceLoadStage::constructing_root;
  try {
    prepare_source_event_names();
    // A prepared camera projection is not a live Window initializer.
    projected_=false;
    // The prepared source graph is not the owner's live child list. No authored
    // resource has been constructed at this boundary; attachment will link it.
    for(auto& node:hierarchy_) node.parent=no_picture_transform_parent;
    for(std::size_t index=1;index<hierarchy_resources_.size();++index) {
      if(hierarchy_resources_[index]) resource_indices_.erase(hierarchy_resources_[index]->value);
      hierarchy_resources_[index].reset();
      resource_owners_[index].reset();
    }
    hierarchy_[0].matrix=engine_identity;
    hierarchy_[0].position={0.0F,0.0F,0.0F};
    resource_states_[0]=IntroRuntimeResourceState{0x01000000U,{}};
    const bool maintenance_suppressed=false;
    set_resource_flags_no_maintenance(resource_handle(root_handle()),0x08000000U,0,
        {resource_allocation_enabled_,maintenance_suppressed});
    root_owner_state_=std::move(metadata);
    components_.construct(0,[this](runtime::ComponentRecord& record) {
      auto& state=record.state();
      state.status|=0x20U;
      root_group_=std::make_shared<RootGroupComponent>(&application_.live_variables(),&application_.input_maps());
      state.class_ordinal=153;
      state.priority=0;
      state.requested=0x115U;
      state.attached_owner=root_handle().value;
      root_group_->bind_owner(RootGroupOwnerHandle{state.attached_owner});
      root_attachments_.push_back(0);
      root_owner_state_->component_mask|=state.requested;
      const auto admitted=state.requested&~state.admitted&0x158U;
      state.requested|=admitted;
      state.admitted|=admitted;
      if(admitted&0x10U) register_ordinary_component(0);
      return runtime::ConstructedComponent{state,
          [payload=root_group_](runtime::ComponentRecord&){payload->initialize();},{}};
    });
    components_.at(0).state().status|=2U;
    root_group_->initialize();
    // Publish the factory result, then enable its separate root marker. Fresh
    // room mode is false and parent absent: no parent/resource mutation follows.
    root_owner_state_->enabled=true;
    resource_load_stage_=IntroResourceLoadStage::root_ready;
  } catch(...) {
    resource_load_stage_=IntroResourceLoadStage::failed;
    throw;
  }
}

std::optional<IntroRuntimeResourceHandle> IntroRuntime::allocated_source_resource(std::size_t source) const {
  const auto index=hierarchy_index(source_handle(source));
  if(!resource_states_[index]) return std::nullopt;
  return hierarchy_resources_[index];
}

void IntroRuntime::begin_source_loading_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::root_ready || loading_progress_)
    throw std::runtime_error("Native source load begin requires a fresh completed root");
  // Explicit native load-begin reset, not an original constructor default.
  loading_progress_=0.0F;
  try {
    // Actual ordinary loader producers: retain service and any queue storage,
    // disable collection, then increment the live suppression word with wrap.
    position_mode_.collection_enabled=false;
    position_mode_.suppression=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(position_mode_.suppression)+std::uint32_t{1});
    directory_position_controls_prepared_=true;
    advance_source_loading_progress_without_engine_renderer(0);
    allocate_initial_source_scope();
  } catch(...) {
    resource_load_stage_=IntroResourceLoadStage::failed;
    throw;
  }
}

float IntroRuntime::advance_source_loading_progress_without_engine_renderer(std::size_t source) {
  if(!loading_progress_ || resource_load_stage_==IntroResourceLoadStage::failed ||
      manager_row_edit_ || scene_resource_edit_ || source!=loaded_resource_handles_.size() ||
      source>=resources_.sources().directory().size())
    throw std::runtime_error("Source progress requires the next directory row and retained load state");
  const auto binary32=[](float value) {volatile float rounded=value;return rounded;};
  const auto fraction=binary32(static_cast<float>(source)/
      static_cast<float>(resources_.sources().directory().size()));
  const auto candidate=binary32(0.8F+binary32(binary32(0.9F-0.8F)*fraction));
  if(candidate>binary32(*loading_progress_+0.002F)) loading_progress_=candidate;
  // No engine renderer exists at this admitted stage. The separate splash
  // cannot substitute for its progress drawing and presentation services.
  return *loading_progress_;
}

void IntroRuntime::allocate_initial_source_scope() {
  if(resource_load_stage_!=IntroResourceLoadStage::root_ready || !source_resource_scopes_.empty() ||
      resource_allocation_enabled_ || components_.construction_mode())
    throw std::runtime_error("Initial source batch requires the completed root and inactive allocation mode");
  resource_load_stage_=IntroResourceLoadStage::allocating_initial_scope;
  try {
    allocate_source_scope(0);
    resource_load_stage_=IntroResourceLoadStage::initial_scope_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::allocate_source_scope(std::uint32_t count_group) {
  if(resource_allocation_enabled_ || components_.construction_mode())
    throw std::runtime_error("Source batch requires inactive allocation mode");
  const auto& groups=resources_.sources().pool_groups();
  if(count_group>=groups.size()) throw std::runtime_error("Source construction count table is exhausted");
  if(std::ranges::any_of(source_resource_scopes_,[&](const auto& scope){return scope.count_group==count_group;}))
    throw std::runtime_error("Source scope was already allocated");
  const auto& group=groups[count_group];
  const auto& directory=resources_.sources().directory();
  // Pair the class-partition allocation order with reserved catalog storage.
  // This table reserves no owner and does not execute later-scope constructors.
  std::vector<std::optional<std::uint32_t>> slot_indices(group.slot_count);
  for(std::size_t source=0;source<directory.size();++source) {
    const auto& entry=directory[source];
    if(entry.pool_group!=count_group) continue;
    if(entry.group_slot_index>=slot_indices.size() || slot_indices[entry.group_slot_index])
      throw std::runtime_error("Invalid initial source resource slot mapping");
    const auto index=hierarchy_index(source_handle(source));
    if(hierarchy_resources_[index] || resource_states_[index] || resource_owners_[index])
      throw std::runtime_error("Source resource was already allocated or associated");
    slot_indices[entry.group_slot_index]=index;
  }
  if(std::ranges::any_of(slot_indices,[](const auto& index){return !index;}))
    throw std::runtime_error("Initial source resource partitions are not fully mapped");
  IntroSourceResourceScope scope;
  scope.count_group=count_group;
  scope.counts=group.class_counts;
  std::uint32_t offset=0;
  for(std::size_t category=0;category<scope.counts.size();++category) {
    if(scope.counts[category]>group.slot_count-offset)
      throw std::runtime_error("Initial source resource partition overflow");
    if(scope.counts[category]) scope.next_in_partition[category]=offset;
    offset+=scope.counts[category];
  }
  if(offset!=group.slot_count) throw std::runtime_error("Initial source resource count mismatch");
  scope.resources.reserve(group.slot_count);
  source_resource_scopes_.reserve(source_resource_scopes_.size()+1);
  try {
    source_resource_scopes_.push_back(std::move(scope));
    // Native arena identities are not original pointers or reserved owner IDs.
    const auto first=group.slot_count?application_.allocate_runtime_owners(group.slot_count):0;
    const bool maintenance_suppressed=false;
    for(std::size_t slot=0;slot<slot_indices.size();++slot) {
      const auto index=*slot_indices[slot];
      const IntroRuntimeResourceHandle resource{first+slot};
      if(!resource_indices_.emplace(resource.value,index).second)
        throw std::runtime_error("Duplicate native source resource identity");
      hierarchy_resources_[index]=resource;
      resource_owners_[index].reset();
      hierarchy_[index]={engine_identity,{0.0F,0.0F,0.0F},no_picture_transform_parent};
      resource_states_[index]=IntroRuntimeResourceState{0x01000000U,{}};
      source_resource_scopes_.back().resources.push_back(resource);
      set_resource_flags_no_maintenance(resource,0x08000000U,0,
          {resource_allocation_enabled_,maintenance_suppressed});
    }
  } catch(...) {
    resource_load_stage_=IntroResourceLoadStage::failed;
    throw;
  }
}

void IntroRuntime::construct_first_authored_group() {
  if(!application_.has_group_registration())
    throw std::runtime_error("First group requires a registered concrete group factory");
  if(resource_load_stage_!=IntroResourceLoadStage::initial_scope_ready || loading_progress_!=0.8F ||
      first_authored_group_ || resource_allocation_enabled_ || components_.construction_mode())
    throw std::runtime_error("First group requires the completed absent-renderer pre-row stage");
  const auto& directory=resources_.sources().directory();
  if(directory.empty()) throw std::runtime_error("First group source is absent");
  const auto& source=directory.front();
  const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
  if(source.source_type!=0x00100001U || source.source_variant || source.parent_steps || source.enters_child_pool ||
      source.object_flags!=0x03200000U || source.class_data_value || source.auxiliary_value ||
      source.buf_auxiliary_offset || source.child_value || source.post_load_source_offset ||
      source.attachment_table_offset || !source.attachments.empty() || !source.deferred_source_offset || source.basis!=engine_identity ||
      !std::ranges::all_of(source.position,zero) || source.pool_group || source.pool_class)
    throw std::runtime_error("Unsupported first authored group source shape");
  const auto root=resource_handle(root_handle());
  const auto& root_state=resource_state_for_handle(root);
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || root_owner_state_->aggregate_flags || !root_owner_state_->enabled ||
      root_owner_state_->room_mode || !child_owners(root_handle()).empty() ||
      hierarchy_[0].parent!=no_picture_transform_parent || hierarchy_[0].matrix!=engine_identity ||
      !std::ranges::all_of(hierarchy_[0].position,zero))
    throw std::runtime_error("First group requires the actual fresh live ROOT");
  auto& scope=source_resource_scopes_.at(0);
  if(!scope.next_in_partition[0] || *scope.next_in_partition[0]!=0 || !scope.counts[0])
    throw std::runtime_error("First group partition is absent or already consumed");
  const auto resource=scope.resources.at(*scope.next_in_partition[0]);
  const auto index=resource_index(resource);
  if(index!=hierarchy_index(source_handle(0)) || resource_owners_[index] ||
      !resource_states_[index] || resource_states_[index]->flags!=0x09000000U ||
      resource_states_[index]->context.value || resource_states_[index]->metadata ||
      resource_states_[index]->directory_auxiliary || hierarchy_[index].parent!=no_picture_transform_parent ||
      hierarchy_[index].matrix!=engine_identity || !std::ranges::all_of(hierarchy_[index].position,zero))
    throw std::runtime_error("First group supplied resource is not fresh and ownerless");
  const auto names=resources_.source_names();
  if(source.buf_name_offset>=names.size()) throw std::runtime_error("Group name is out of range");
  std::size_t end=source.buf_name_offset;
  while(end<names.size() && names[end]!=std::byte{0}) ++end;
  if(end==names.size()) throw std::runtime_error("Group name is unterminated");
  try {
    ++*scope.next_in_partition[0]; // Consume supplied slot BEFORE concrete factory allocation.
    std::string name;
    for(std::size_t i=source.buf_name_offset;i<end;++i) name.push_back(static_cast<char>(names[i]));
    first_authored_group_=IntroAuthoredGroupOwner{source_handle(0),resource,std::move(name)};
    resource_owners_[index]=source_handle(0);
    const auto previous_count=application_.register_group_instance();
    // The registered concrete ZGROUP notification accepts the previous count
    // and has no effects; this does not stand in for other class notifications.
    static_cast<void>(previous_count);
    manager_row_edit_=true;
    scene_resource_edit_=true;
    loaded_resource_handles_.push_back(resource);
    resource_states_[index]->metadata=0;
    resource_states_[index]->directory_auxiliary=0;
    // Exact equal transform service branch: no dirty write or queue call.
    // Concrete parent selection is ROOT for this admitted first-source route.
    auto merged=resource_states_[index]->flags|(source.object_flags&0xfffffU);
    if(merged&0x8080U) merged|=0x8080U;
    const bool suppressed=false;
    set_resource_flags_no_maintenance(resource,merged,~merged,{resource_allocation_enabled_,suppressed});
    // Empty ROOT group insertion: canonical parent establishes its sole child,
    // hence head/tail/count, without sibling or category/spatial side effects.
    hierarchy_[index].parent=resource_index(root);
    const auto current=resource_states_[index]->flags;
    set_resource_flags_no_maintenance(resource,current,~current,{resource_allocation_enabled_,suppressed});
    ++count_group_selector_;
    first_authored_group_->source_word=source.child_value;
    first_authored_group_->flags=(first_authored_group_->flags&0x00ffffffU)|(source.object_flags&0xff000000U);
    deferred_reader_work_.push_back({resource,source.deferred_source_offset,0,false,{}});
    directory_resource_mapping_.resize(directory.size());
    directory_resource_mapping_[0]=resource;
    manager_row_edit_=false;
    scene_resource_edit_=false;
    resource_load_stage_=IntroResourceLoadStage::first_group_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

std::optional<IntroSceneResourceProperty> IntroRuntime::scene_resource_property(std::string_view key) const {
  if(key.find('\0')!=std::string_view::npos) throw std::runtime_error("Scene property key contains NUL");
  const auto found=scene_resource_properties_.find(key);
  if(found==scene_resource_properties_.end()) return std::nullopt;
  return found->second;
}

void IntroRuntime::set_scene_resource_property_native(std::string key,IntroRuntimeResourceHandle resource) {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Resource construction previously failed");
  if(key.find('\0')!=std::string::npos) throw std::runtime_error("Scene property key contains NUL");
  if(!resource_state_for_handle(resource)) throw std::runtime_error("Scene property requires an allocated resource");
  scene_resource_properties_.erase(key);
  scene_resource_properties_.emplace(std::move(key),IntroSceneResourceProperty{16,resource});
}

void IntroRuntime::construct_window_language_groups_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::first_group_ready || window_owner_ || language_owner_ ||
      resource_allocation_enabled_ || components_.construction_mode() || loaded_resource_handles_.size()!=1 ||
      count_group_selector_!=1 || current_source_parent()!=root_handle() || manager_row_edit_ || scene_resource_edit_)
    throw std::runtime_error("Window and Language require the completed first group stage");
  if(!application_.has_class_registration(0x00100030U) || !application_.has_class_registration(0x00101389U))
    throw std::runtime_error("Window and Language require concrete class registration");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<3) throw std::runtime_error("Window or Language source is absent");
  const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
  // Validate the bounded source shapes before consuming any factory slot.
  for(std::size_t row=1;row<=2;++row) {
    const auto& source=directory[row];
    if(source.source_type!=(row==1?0x00100030U:0x00101389U) || source.source_variant || source.parent_steps ||
        !source.enters_child_pool || source.object_flags!=0x03000000U || source.class_data_value ||
        source.auxiliary_value || source.buf_auxiliary_offset || source.child_value || source.post_load_source_offset ||
        source.attachment_table_offset || !source.attachments.empty() || !source.deferred_source_offset ||
        source.basis!=engine_identity || !std::ranges::all_of(source.position,zero) || source.pool_class ||
        source.pool_group!=(row==1?0U:2U))
      throw std::runtime_error("Unsupported Window or Language source shape");
  }
  const auto root=resource_handle(root_handle());
  const auto& root_state=resource_state_for_handle(root);
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->aggregate_flags || root_owner_state_->room_mode ||
      hierarchy_[0].parent!=no_picture_transform_parent || hierarchy_[0].matrix!=engine_identity ||
      !std::ranges::all_of(hierarchy_[0].position,zero) ||
      child_owners(root_handle())!=std::vector<IntroRuntimeHandle>{source_handle(0)})
    throw std::runtime_error("Window construction requires the retained fresh ROOT ancestry");
  try {
    for(std::size_t row=1;row<=2;++row) {
      advance_source_loading_progress_without_engine_renderer(row);
      if(row==2) allocate_source_scope(count_group_selector_);
      construct_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::window_language_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_group_row_without_engine_renderer(std::size_t row) {
      const auto& source=resources_.sources().directory().at(row);
      if(source.source_type!=0x00100030U && source.source_type!=0x00101389U && source.source_type!=0x00100001U && source.source_type!=0x00100021U && source.source_type!=0x0010002eU && source.source_type!=0x00100031U)
        throw std::runtime_error("Unsupported registered group factory");
      if(!application_.has_class_registration(source.source_type))
        throw std::runtime_error("Group concrete class is not registered");
      if(window_owners_.contains(row) || constructed_group_owners_.contains(row) ||
          (source.source_type==0x00101389U && language_owner_))
        throw std::runtime_error("Group owner is already constructed");
      const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
      const auto found=std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group);
      if(found==source_resource_scopes_.end()) throw std::runtime_error("Group allocation scope is absent");
      auto& scope=*found;
      const auto partition=source.pool_class;
      if(partition>=scope.counts.size() || partition%8!=0) throw std::runtime_error("Invalid banked group partition");
      auto& cursor=scope.next_in_partition[partition];
      std::uint64_t partition_begin=0;
      for(std::size_t i=0;i<partition;++i) partition_begin+=scope.counts[i];
      if(!cursor || *cursor<partition_begin || *cursor>=partition_begin+scope.counts[partition]) throw std::runtime_error("Group partition is exhausted");
      const auto resource=scope.resources.at(*cursor);
      const auto index=resource_index(resource);
      const auto parent=current_source_parent();
      const auto parent_resource=resource_handle(parent);
      const auto& parent_state=resource_state_for_handle(parent_resource);
      if(!parent_state || (parent_state->flags&~0x001000ffU)!=0x09000000U || parent_state->context.value ||
          index!=hierarchy_index(source_handle(row)) || resource_owners_[index] || !resource_states_[index] ||
          resource_states_[index]->flags!=0x09000000U || resource_states_[index]->context.value ||
          resource_states_[index]->metadata || resource_states_[index]->directory_auxiliary ||
          hierarchy_[index].parent!=no_picture_transform_parent || !engine_identity_bits(hierarchy_[index].matrix) ||
          !std::ranges::all_of(hierarchy_[index].position,zero))
        throw std::runtime_error("Group attachment requires fresh canonical resource and parent state");
      const auto names=resources_.source_names();
      if(source.buf_name_offset>=names.size()) throw std::runtime_error("Group name is out of range");
      std::size_t end=source.buf_name_offset;
      while(end<names.size() && names[end]!=std::byte{0}) ++end;
      if(end==names.size()) throw std::runtime_error("Group name is unterminated");
      ++*cursor; // The owner factory consumes the already allocated resource.
      std::string name;
      for(std::size_t i=source.buf_name_offset;i<end;++i) name.push_back(static_cast<char>(names[i]));
      const auto owner=source_handle(row);
      IntroAuthoredGroupOwner* group=nullptr;
      if(source.source_type==0x00100030U) {
        auto window=std::make_unique<IntroWindowOwner>();
        window->group={owner,resource,std::move(name),source.source_type};
        auto& stored=*window_owners_.emplace(row,std::move(window)).first->second;
        if(row==resources_.window_index()) window_owner_=&stored;
        group=&stored.group;
        resource_owners_[index]=owner;
        stored.show_2d=application_.live_variables().bind("Show2d",stored.pending_visibility);
        set_scene_resource_property_native("rWindows",resource);
      } else if(source.source_type==0x00101389U) {
        language_owner_=IntroAuthoredGroupOwner{owner,resource,std::move(name),source.source_type};
        group=&*language_owner_;
        resource_owners_[index]=owner;
      } else if(source.source_type==0x00100021U) {
        auto& room=constructed_room_owners_.try_emplace(row).first->second;
        room.group={owner,resource,std::move(name),source.source_type};
        room.group.flags=0;
        group=&room.group;
        resource_owners_[index]=owner;
      } else {
        group=&constructed_group_owners_.emplace(row,IntroAuthoredGroupOwner{owner,resource,std::move(name),source.source_type}).first->second;
        resource_owners_[index]=owner;
      }
      if(source.source_type==0x0010002eU || source.source_type==0x00100031U) group->cached_window=IntroRuntimeHandle{};
      if(source.source_type==0x00100031U) group->derived_reference=IntroRuntimeHandle{};
      const auto previous=application_.register_class_instance(source.source_type);
      static_cast<void>(previous); // Both concrete notifications have no additional effects.
      manager_row_edit_=true;
      scene_resource_edit_=true;
      loaded_resource_handles_.push_back(resource);
      assign_fresh_directory_metadata(resource,source.class_data_value);
      if(source.buf_auxiliary_offset) assign_directory_property(row);
      resource_states_[index]->directory_auxiliary=source.auxiliary_value;
      apply_directory_transform(row,resource);
      attach_directory_owner(row,resource,parent);
      ++count_group_selector_;
      current_source_parent_=owner;
      group->source_word=source.child_value;
      group->flags=(group->flags&0x00ffffffU)|(source.object_flags&0xff000000U);
      if(!source.attachments.empty()) {
        auto& auxiliary=ensure_owner_auxiliary(row);
        scene_resource_edit_=false;
        construct_owner_attachments(row,auxiliary.component_mask,auxiliary.attachments);
        group->component_mask=auxiliary.component_mask;
        scene_resource_edit_=true;
      }
      if(source.deferred_source_offset) deferred_reader_work_.push_back({resource,source.deferred_source_offset,row,false,{}});
      directory_resource_mapping_.at(row)=resource;
      manager_row_edit_=false;
      scene_resource_edit_=false;
}

std::uint16_t IntroRuntime::declare_scene_event_name(std::string_view name,std::uint16_t requested) {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Cannot declare scene events after failed source construction");
  return event_names_.declare(name,requested);
}

void IntroRuntime::prepare_source_event_names() {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Cannot prepare scene events after failed source construction");
  if(source_event_names_prepared_) return;
  if(resource_load_stage_!=IntroResourceLoadStage::prepared && resource_load_stage_!=IntroResourceLoadStage::constructing_root)
    throw std::runtime_error("Source event preparation must precede ROOT construction");
  const auto count=resources_.sources().identifier_count();
  if(count>=std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("Source event mapping is too large");
  try {
    source_event_name_mapping_.resize(count+1);
    for(std::size_t index=1;index<=count;++index) {
      const auto name=resources_.sources().authored_event_identifier(static_cast<std::uint32_t>(index));
      if(!name) throw std::runtime_error("Source event name is absent");
      source_event_name_mapping_[index]=event_names_.declare(*name);
    }
    source_event_names_prepared_=true;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

const IntroConstructedPictureOwner* IntroRuntime::constructed_picture_owner(std::size_t source) const noexcept {
  const auto found=constructed_picture_owners_.find(source);
  return found==constructed_picture_owners_.end()?nullptr:&found->second;
}
const IntroConstructedPictureComponent* IntroRuntime::constructed_picture_component(std::size_t source) const noexcept {
  if(source>=resources_.sources().directory().size()) return nullptr;
  const auto& list=owner_components_.at(hierarchy_index(source_handle(source)));
  return list.empty()?nullptr:constructed_attachment(list.front());
}

void IntroRuntime::construct_picture_component_prefix_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::window_language_ready || !window_owner_ || !language_owner_ ||
      !constructed_picture_owners_.empty() || resource_allocation_enabled_ || components_.construction_mode() ||
      loaded_resource_handles_.size()!=3 || count_group_selector_!=3 || current_source_parent()!=language_owner_->owner ||
      manager_row_edit_ || scene_resource_edit_)
    throw std::runtime_error("Picture prefix requires the completed Window and Language stage");
  if(!application_.has_class_registration(0x00200046U) ||
      !application_.has_component_class_registration("ZGEOM_Center") ||
      !application_.has_component_class_registration("ZWINPIC_FadeToBlack"))
    throw std::runtime_error("Picture prefix requires concrete owner and component registrations");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<5) throw std::runtime_error("Picture prefix sources are absent");
  const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
  const auto& root_state=resource_state(root_handle());
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
      root_owner_state_->aggregate_flags || !root_owner_state_->category_memberships.empty() ||
      window_owner_->group.aggregate_flags || language_owner_->aggregate_flags ||
      resource_parent(window_owner_->group.owner)!=resource_handle(root_handle()) ||
      resource_parent(language_owner_->owner)!=resource_handle(window_owner_->group.owner) ||
      child_owners(window_owner_->group.owner)!=std::vector<IntroRuntimeHandle>{language_owner_->owner} ||
      !child_owners(language_owner_->owner).empty())
    throw std::runtime_error("Picture prefix requires the retained fresh non-room ancestry");
  for(std::size_t row=3;row<=4;++row) {
    const auto& source=directory[row];
    const auto expected=row==3?"ZGEOM_Center":"ZWINPIC_FadeToBlack";
    if(source.source_type!=0x00200046U || source.source_variant || source.parent_steps!=(row==3?0:1) ||
        source.enters_child_pool || source.object_flags!=(row==3?0x00200400U:0x00200000U) ||
        source.class_data_value || source.auxiliary_value || source.buf_auxiliary_offset || source.child_value ||
        source.post_load_source_offset || !source.deferred_source_offset || source.attachments.size()!=1 ||
        source.basis!=engine_identity || !std::ranges::all_of(source.position,zero) || source.pool_class!=1 ||
        source.pool_group!=(row==3?3U:2U) || resources_.sources().attachment_identifier(row,0)!=expected ||
        source.attachments.front().parameter!=(row==3?1.0F:0.0F) || owner_components(source_handle(row)).size()!=1)
      throw std::runtime_error("Unsupported Picture prefix source shape");
  }
  try {
    for(std::size_t row=3;row<=4;++row) {
      advance_source_loading_progress_without_engine_renderer(row);
      if(row==3) allocate_source_scope(count_group_selector_);
      else current_source_parent_=window_owner_->group.owner;
      construct_picture_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::picture_component_prefix_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_picture_row_without_engine_renderer(std::size_t row) {
  construct_non_group_row_without_engine_renderer(row);
}

void IntroRuntime::construct_non_group_row_without_engine_renderer(std::size_t row) {
      const auto& source=resources_.sources().directory().at(row);
      if(!application_.has_class_registration(source.source_type))
        throw std::runtime_error("Concrete non-group factory is not registered");
      if(source.source_type!=0x00200046U && source.source_type!=0x0020002dU && source.source_type!=0x0020003aU &&
          source.source_type!=0x0800001aU && source.source_type!=0x00400003U && source.source_type!=0x00200002U &&
          source.source_type!=0x00800024U && source.source_type!=0x002000e5U && source.source_type!=0x0080000dU &&
          source.source_type!=0x0020000bU && source.source_type!=0x00800023U && source.source_type!=0x04000022U &&
          source.source_type!=0x00800020U && source.source_type!=0x08000049U && source.source_type!=0x00200012U &&
          source.source_type!=0x002000e4U)
        throw std::runtime_error("Unsupported concrete non-group factory");
      const auto& root_state=resource_state(root_handle());
      if(!root_state || (root_state->flags&~0xffU)!=0x09000000U || root_state->context.value ||
          !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
          (root_owner_state_->aggregate_flags&~0x10000U) || !root_owner_state_->category_memberships.empty())
        throw std::runtime_error("Non-group construction requires the retained fresh non-room ROOT");
      const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
      const auto scope_it=std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group);
      if(scope_it==source_resource_scopes_.end()) throw std::runtime_error("Picture allocation scope is absent");
      auto& scope=*scope_it;
      const auto category=source.pool_class;
      if(category>=scope.counts.size() || (category%8!=1 && category%8!=2 && category%8!=3)) throw std::runtime_error("Unsupported non-group partition");
      auto& cursor=scope.next_in_partition[category];
      std::uint64_t partition_begin=0;
      for(std::size_t i=0;i<category;++i) partition_begin+=scope.counts[i];
      const auto partition_end=partition_begin+scope.counts[category];
      if(!cursor || *cursor<partition_begin || *cursor>=partition_end)
        throw std::runtime_error("Picture partition is absent or exhausted");
      const auto resource=scope.resources.at(*cursor);
      const auto index=resource_index(resource);
      const auto owner=source_handle(row);
      const auto parent_resource=resource_handle(current_source_parent());
      const auto& parent_state=resource_state_for_handle(parent_resource);
      if(index!=hierarchy_index(owner) || resource_owners_[index] || !resource_states_[index] ||
          resource_states_[index]->flags!=0x09000000U || resource_states_[index]->context.value ||
          resource_states_[index]->metadata || resource_states_[index]->directory_auxiliary ||
          hierarchy_[index].parent!=no_picture_transform_parent || !engine_identity_bits(hierarchy_[index].matrix) ||
          !std::ranges::all_of(hierarchy_[index].position,zero) || !parent_state ||
          (parent_state->flags&~0x001000ffU)!=0x09000000U || parent_state->context.value)
        throw std::runtime_error("Picture attachment requires fresh canonical resource and parent state");
      const auto names=resources_.source_names();
      if(source.buf_name_offset>=names.size()) throw std::runtime_error("Picture name is out of range");
      std::size_t end=source.buf_name_offset;
      while(end<names.size() && names[end]!=std::byte{0}) ++end;
      if(end==names.size()) throw std::runtime_error("Picture name is unterminated");
      ++*cursor;
      std::string name;
      for(std::size_t i=source.buf_name_offset;i<end;++i) name.push_back(static_cast<char>(names[i]));
      std::uint32_t* mask=nullptr;
      std::vector<std::uint64_t>* attachments=nullptr;
      if(source.source_type==0x00200046U || source.source_type==0x0020002dU || source.source_type==0x0020003aU) {
        auto& visual=source.source_type==0x00200046U?
            constructed_picture_owners_.try_emplace(row).first->second:
            source.source_type==0x0020002dU?constructed_character_owners_.try_emplace(row).first->second.visual:
            constructed_visual_owners_.try_emplace(row).first->second;
        visual.owner=owner;visual.resource=resource;visual.name=std::move(name);visual.class_identifier=source.source_type;
        mask=&visual.component_mask;attachments=&visual.attachments;
      } else if(source.source_type==0x0800001aU || source.source_type==0x08000049U) {
        auto& list=constructed_list_owners_.try_emplace(row).first->second;
        list.owner=owner;list.resource=resource;list.name=std::move(name);list.class_identifier=source.source_type;
        mask=&list.component_mask;attachments=&list.attachments;
        if(source.source_type==0x08000049U) {
          resource_owners_[index]=owner;
          const auto property=scene_resource_property("rWINOBJSPRITEHOLDER");
          if(property && property->owner_handle && live_owner(property->owner_handle->value) && *property->owner_handle!=owner)
            throw std::runtime_error("Sprite holder scene property already resolves to a different live owner");
          set_scene_owner_property_native("rWINOBJSPRITEHOLDER",owner,4);
          list.animation_storage=std::make_unique<IntroConstructedListOwner::AnimationStorage>();
        }
      } else if(source.source_type==0x00400003U) {
        auto camera=std::make_unique<IntroLiveCameraOwner>();
        camera->metadata={owner,resource,std::move(name)};camera->context=root_handle();
        live_cameras_.emplace(row,std::move(camera));
      } else {
        auto& object=constructed_object_owners_.try_emplace(row).first->second;
        object.owner=owner;object.resource=resource;object.name=std::move(name);object.class_identifier=source.source_type;
        if((source.source_type==0x00800024U || source.source_type==0x0080000dU || source.source_type==0x00800023U || source.source_type==0x00800020U) && light_policy_) object.flags|=1;
        if(source.source_type==0x0020000bU) object.byte_control=false;
        if(source.source_type==0x0080000dU) {
          object.local_control=0x3000U;
          object.self_links=std::array{owner,owner};
        }
        if(source.source_type==0x00800024U || source.source_type==0x00800023U || source.source_type==0x04000022U || source.source_type==0x00800020U) object.local_reference=IntroRuntimeHandle{};
        if(source.source_type==0x04000022U) {
          object.scalar_pair=std::array{-1.0F,std::bit_cast<float>(0x7e967699U)};
          object.associated_references=std::array<IntroRuntimeHandle,2>{};
        }
        resource_owners_[index]=owner;
        if(source.source_type==0x00200012U) {
          if(sound_load_policy_!=IntroSoundLoadPolicy::directory_construction)
            throw std::runtime_error("Directory sound construction requires the unbound sound catalog policy");
          set_resource_flags_no_maintenance(resource,0,0x38003U,{resource_allocation_enabled_,false});
          auto events=application_.sound_owner_events();
          constexpr std::array<std::string_view,3> names{"DeleteSound","ActivateSound","DeactivateSound"};
          for(std::size_t event=0;event<names.size();++event) {
            events[event]=event_names_.declare(names[event]);
            application_.set_sound_owner_events(events);
          }
          auto& sound=sound_for_source(row);
          if(sound.has_record() || sound.owner_binding_ || sound.active_)
            throw std::runtime_error("Directory sound factory requires fresh retained owner state");
          if(auto* backend=application_.sound_record_backend();backend && !restore_mode_)
            sound.lease_=backend->create(owner.value);
        }
        if(source.source_type==0x002000e4U) {
          object.particle_usage=std::make_unique<IntroConstructedObjectOwner::ParticleUsageState>();
          auto& usage=*object.particle_usage;
          usage.activate_event=event_names_.declare("Activate");
          usage.pool=std::make_unique<IntroConstructedObjectOwner::ParticleUsageState::Pool>();
          for(std::size_t slot=0;slot<usage.pool->available_slots.size();++slot)
            usage.pool->available_slots[slot]=static_cast<std::uint32_t>(slot);
          usage.descriptor=std::make_unique<IntroConstructedObjectOwner::ParticleUsageState::ConsoleDescriptor>();
          usage.descriptor->target=&usage.diagnostic_enabled;
          if(auto* console=application_.optional_console())
            usage.descriptor->registration=console->bind(usage.descriptor->key,usage.diagnostic_enabled);
        }
        if(source.source_type==0x002000e5U) {
          const auto property=scene_resource_property("ParticleTemplates");
          auto* collection=property && property->object_token?application_.resolve_handle_collection(*property->object_token):nullptr;
          if(!collection) {
            const auto token=application_.create_handle_collection();
            collection=application_.resolve_handle_collection(token);
            if(!collection) throw std::runtime_error("ParticleTemplates collection token is not live");
            set_scene_object_property_native("ParticleTemplates",token);
          }
          collection->members.push_back(owner.value);
        }
        if(!source.attachments.empty()) {
          object.auxiliary=std::make_unique<IntroOwnerAuxiliary>();
          mask=&object.auxiliary->component_mask;attachments=&object.auxiliary->attachments;
        }
      }
      resource_owners_[index]=owner;
      const auto notification=application_.register_class_instance(source.source_type);
      if(source.source_type==0x00400003U) live_cameras_.at(row)->metadata.notification_sequence=notification;
      manager_row_edit_=true;scene_resource_edit_=true;
      loaded_resource_handles_.push_back(resource);
      assign_fresh_directory_metadata(resource,source.class_data_value);
      if(source.buf_auxiliary_offset) assign_directory_property(row);
      // Authored renderer identifier, not a relocated pointer. Cold flag and
      // position paths preserve the full word after this ordered assignment.
      resource_states_[index]->directory_auxiliary=source.auxiliary_value;
      apply_directory_transform(row,resource);
      attach_directory_owner(row,resource,current_source_parent());
      if(!source.attachments.empty()) {
        if(source.source_type==0x00400003U) {
          auto& auxiliary=ensure_owner_auxiliary(row);
          mask=&auxiliary.component_mask;attachments=&auxiliary.attachments;
        }
        if(!mask || !attachments) throw std::runtime_error("Camera attachments are not admitted");
        scene_resource_edit_=false;
        construct_owner_attachments(row,*mask,*attachments);
        scene_resource_edit_=true;
      }
      if(source.deferred_source_offset) deferred_reader_work_.push_back({resource,source.deferred_source_offset,row,false,{}});
      directory_resource_mapping_.at(row)=resource;
      manager_row_edit_=false;scene_resource_edit_=false;
}

void IntroRuntime::construct_owner_attachments(std::size_t row,std::uint32_t& mask,std::vector<std::uint64_t>& attachments) {
      const auto owner=source_handle(row);
      const auto index=hierarchy_index(owner);
      const auto& source=resources_.sources().directory().at(row);
      const auto indices=owner_components(owner);
      if(indices.size()!=source.attachments.size()) throw std::runtime_error("Owner attachment catalog mismatch");
      for(std::size_t slot=0;slot<indices.size();++slot) {
      const auto component_index=indices[slot];
      const auto factory=resources_.sources().attachment_identifier(row,slot);
      const bool center=factory=="ZGEOM_Center";
      const bool black=factory=="ZWINPIC_FadeToBlack";
      const bool character=factory=="ZCHAROBJ_CharFader";
      const bool logo=factory=="ZWINPIC_LogoFade";
      const bool external=factory=="ZLIST_ExternCutSequenceCommand";
      const bool vert=factory=="ZSTDOBJ_VertAnim";
      const bool mat=factory=="ZGEOM_MatPosAnim";
      const bool cut=factory=="ZLIST_CutSequence";
      const bool cut_list=factory=="ZLIST_CutSequenceList";
      const bool grain=factory=="ZGEOM_FilmGrainCamSetup";
      const bool flare_control=factory=="ZWINDOW_LensFlareControl";
      const bool flare=factory=="ZWINPIC_LensFlare";
      const bool param=factory=="ZGEOM_ParamAnim";
      const bool emitter=factory=="ZGEOM_ParticleEmitter";
      const bool flare_lights=factory=="ZLIST_LensFlareLights";
      const bool scroll=factory=="ZSTDOBJ_ScrollTexture";
      const bool command=factory=="ZLIST_CutSequenceCommand";
      const bool movie=factory=="ZGEOM_MovieControl";
      const bool sound_extend=factory=="ZSNDOBJ_SoundExtend";
      const bool sound_notify=factory=="ZSNDOBJ_SoundNotify";
      const bool sound_segment=factory=="ZSNDOBJ_SoundSegment";
      const bool define=factory=="ZGEOM_ZSetZDefine";
      if((!center && !black && !character && !logo && !external && !vert && !mat && !cut && !cut_list &&
          !grain && !flare_control && !flare && !param && !emitter && !flare_lights && !scroll && !command &&
          !movie && !sound_extend && !sound_notify && !sound_segment && !define) ||
          !application_.has_component_class_registration(factory))
        throw std::runtime_error("Actual attachment factory is unavailable");
      components_.construct(component_index,[&](runtime::ComponentRecord& record) {
        auto& state=record.state();
        state.status|=0x20U;
        auto& payload=constructed_picture_components_.try_emplace(component_index).first->second;
        payload.raw_attachment_argument=std::bit_cast<std::uint32_t>(source.attachments[slot].parameter);
        payload.attachment_argument=external || vert || mat || cut || param || emitter || command || sound_segment?std::bit_cast<std::int32_t>(payload.raw_attachment_argument):center?1:0;
        state.class_ordinal=static_cast<std::uint16_t>(center?280:black?307:character?332:logo?313:external?112:vert?85:mat?86:cut?96:cut_list?97:grain?308:flare_control?311:flare?310:param?80:emitter?145:flare_lights?312:scroll?160:command?106:movie?314:sound_extend?133:sound_notify?121:sound_segment?136:165);
        state.priority=vert || mat || param?100U:center || external || cut || command || sound_segment?1U:0U;
        state.requested=center?1U:external || command?0x803U:mat?0x435U:cut?0x825U:cut_list?0x837U:grain || flare_lights?7U:flare?0x25U:param?0x15U:emitter?0x805U:scroll?0x815U:movie?0x37U:sound_extend?0x13U:sound_notify?0x11U:sound_segment?0x17U:define?0x205U:0x35U;
        if(scroll) {
          auto& local=payload.scroll_texture.emplace();
          local.start_event=event_names_.declare("MSG_StartAnimation");
          local.stop_event=event_names_.declare("MSG_StopAnimation");
        }
        if(command) payload.command_text.emplace();
        if(movie) {
          auto& local=payload.movie_control.emplace();
          constexpr std::array<std::string_view,7> names{
              "msg_SoundReady","CutSequence_End","CutSequence_Start","Activate","AddSubtitle","Msg_SayDialog","StopMovieCut"};
          for(std::size_t event=0;event<names.size();++event) local.events[event]=event_names_.declare(names[event]);
        }
        if(sound_extend) payload.sound_extend.emplace().start_event=event_names_.declare("MSG_ANIMSOUNDSTART");
        if(sound_notify) payload.sound_notify.emplace();
        if(sound_segment) {
          auto& local=payload.sound_segment.emplace();
          constexpr std::array<std::string_view,4> names{
              "MSG_SOUNDSEGMENTSTART","MSG_SOUNDSEGMENTSTOP","MSG_WRITESUBTITLE","SubTitlesClear"};
          for(std::size_t event=0;event<names.size();++event) local.events[event]=event_names_.declare(names[event]);
        }
        if(define) payload.sound_define.emplace();
        if(flare_control) components_.construct_and_destroy_temporary_common();
        if(param) payload.param_animation.emplace();
        if(emitter) {
          payload.particle_emitter.emplace();
          application_.set_particle_emitter_event(event_names_.declare("FrameCurrent"));
        }
        if(black || character || logo) {payload.fade_start=0;payload.fade_deadline=0;}
        if(black) payload.fade_state=3;
        if(external) {payload.target_name="";payload.script_reference=0;}
        if(vert || mat) {
          payload.animation.emplace();
          if(mat) {
            payload.animation->mat.emplace();
            payload.animation->enabled_c=true;
            payload.animation->scalar=50;
            payload.animation->word_control=1;
            payload.animation->basis=engine_identity;
            payload.animation->transient_vector=std::array<float,3>{};
          } else {
            payload.animation->vert.emplace();
            application_.set_vert_anim_event(event_names_.declare("Msg_RunWhenPaused"));
          }
        }
        if(cut_list) {
          payload.cut_list.emplace();
          payload.commands.emplace();payload.auxiliary_list_a.emplace();payload.auxiliary_list_b.emplace();
          payload.list_sentinel=-1;
        }
        if(black || character) {
          // The scene registry supplies these values; event declaration is
          // construction work, not event dispatch or an alpha mutation.
          payload.fade_in_event=event_names_.declare("FadeIn");
          payload.fade_out_event=event_names_.declare("FadeOut");
        }
        payload.owner=owner;state.attached_owner=owner.value;
        attachments.push_back(component_handle(component_index));
        const bool hidden=(resource_states_[index]->flags&0x400U)!=0;
        if(state.requested && (!hidden || (state.requested&0x200U))) {
          mask|=state.requested;
          const auto admitted=state.requested&~state.admitted&0x158U;
          state.requested|=admitted;
          if(!hidden) {
            state.admitted|=admitted;
            if(admitted&0x10U) register_ordinary_component(component_index);
          }
        }
        runtime::ComponentCallback phase_one=[](auto&){
          throw std::runtime_error("Constructed attachment requires its source reader and live initialization services");
        };
        // The two authored sound owners are the only independently recovered
        // first-phase family.  These callbacks deliberately stay attached to
        // their concrete factory/source pair; every other component continues
        // to fail closed above.
        if(sound_extend) phase_one=[this,row,component_index](runtime::ComponentRecord& callback_record) {
          auto& component=components_.at(component_index);
          auto& sound=sound_for_source(row);
          const auto payload=constructed_picture_components_.find(component_index);
          if(&callback_record!=&component || component.source().factory_name!="ZSNDOBJ_SoundExtend" ||
              component.state().attached_owner!=sound.handle().value || !sound.source_applied_ ||
              payload==constructed_picture_components_.end() || !payload->second.sound_extend || !sound.active_)
            throw std::runtime_error("SoundExtend phase one requires its prepared typed sound owner");
          apply_sound_extension(row);
          if(!restore_mode_ && (component.state().admitted&0x10U)) {
            component.state().admitted&=~0x10U;
            if(ordinary_) ordinary_->notify_removal();
            payload->second.sound_extend->phase_one_ordinary_removed=true;
          }
          record_supported_component_admission(component_index);
        };
        if(sound_notify) phase_one=[this,row,component_index](runtime::ComponentRecord& callback_record) {
          auto& component=components_.at(component_index);
          auto& sound=sound_for_source(row);
          const auto payload=constructed_picture_components_.find(component_index);
          if(&callback_record!=&component || component.source().factory_name!="ZSNDOBJ_SoundNotify" ||
              component.state().attached_owner!=sound.handle().value || !sound.source_applied_ ||
              payload==constructed_picture_components_.end() || !payload->second.sound_notify || !sound.active_ ||
              !std::isfinite(sound.record().duration))
            throw std::runtime_error("SoundNotify phase one requires its prepared typed sound owner");
          payload->second.sound_notify->duration_snapshot=sound.record().duration;
          record_supported_component_admission(component_index);
        };
        if(sound_segment) phase_one=[this,row,component_index](runtime::ComponentRecord& callback_record) {
          auto& component=components_.at(component_index);
          auto& sound=sound_for_source(row);
          const auto payload=constructed_picture_components_.find(component_index);
          if(&callback_record!=&component || component.source().factory_name!="ZSNDOBJ_SoundSegment" ||
              component.state().attached_owner!=sound.handle().value || !sound.source_applied_ ||
              payload==constructed_picture_components_.end() || !payload->second.sound_segment || !sound.active_ ||
              !std::isfinite(sound.record().duration))
            throw std::runtime_error("SoundSegment phase one requires its prepared typed sound owner");
          // Segment has no independently recovered mutation in this pass; the
          // scoped lifecycle commits its completed status after this concrete
          // typed-owner and parsed-payload validation.
          record_supported_component_admission(component_index);
        };
        if(define) phase_one=[this,row,component_index](runtime::ComponentRecord& callback_record) {
          auto& component=components_.at(component_index);
          auto& sound=sound_for_source(row);
          const auto payload=constructed_picture_components_.find(component_index);
          if(&callback_record!=&component || component.source().factory_name!="ZGEOM_ZSetZDefine" ||
              component.state().attached_owner!=sound.handle().value || !sound.source_applied_ ||
              payload==constructed_picture_components_.end() || !payload->second.sound_define || !sound.active_ ||
              payload->second.sound_define->property_on_parent || payload->second.sound_define->property_key.empty())
            throw std::runtime_error("ZSetZDefine phase one requires its prepared typed sound owner");
          set_scene_owner_property_native(payload->second.sound_define->property_key,sound.handle(),2U);
          record_supported_component_admission(component_index);
        };
        return runtime::ConstructedComponent{state,std::move(phase_one),
            [](auto&){throw std::runtime_error("Constructed attachment requires its live second-phase services");}};
      });
      const auto notification=application_.register_component_class_instance(components_.at(component_index).source().factory_name);
      if(cut_list) {
        if(notification==0) application_.create_cut_sequence_list_collection();
        constructed_picture_components_.at(component_index).list_index=notification;
      }
      }
}

void IntroRuntime::assign_fresh_directory_metadata(IntroRuntimeResourceHandle resource,std::uint32_t metadata) {
  auto& state=resource_states_.at(resource_index(resource));
  if(!state || state->metadata)
    throw std::runtime_error("Nonzero previous directory metadata requires renderer cleanup services");
  // The old-zero guard skips cleanup. Metadata dirtying does not notify the
  // position service and is independent of the subsequent transform equality.
  const auto previous=state->metadata;
  state->metadata=metadata;
  if(metadata!=previous) state->flags|=0x00100000U;
}

void IntroRuntime::set_scene_object_property_native(std::string key,std::uint64_t token) {
  if(resource_load_stage_==IntroResourceLoadStage::failed || key.empty() || key.find('\0')!=std::string::npos ||
      !application_.resolve_handle_collection(token))
    throw std::runtime_error("Scene object property requires a live application collection token");
  scene_resource_properties_.erase(key);
  scene_resource_properties_.emplace(std::move(key),IntroSceneResourceProperty{16,{},token});
}
void IntroRuntime::set_scene_owner_property_native(std::string key,IntroRuntimeHandle owner,std::uint32_t flags) {
  if(resource_load_stage_==IntroResourceLoadStage::failed || key.empty() || key.find('\0')!=std::string::npos ||
      !live_owner(owner.value))
    throw std::runtime_error("Scene owner property requires a live canonical owner");
  scene_resource_properties_.erase(key);
  scene_resource_properties_.emplace(std::move(key),IntroSceneResourceProperty{16,{},std::nullopt,owner,flags});
}
IntroOwnerAuxiliary& IntroRuntime::ensure_owner_auxiliary(std::size_t source) {
  if(const auto camera=live_cameras_.find(source);camera!=live_cameras_.end()) {
    if(!camera->second->auxiliary) camera->second->auxiliary=std::make_unique<IntroOwnerAuxiliary>();
    return *camera->second->auxiliary;
  }
  if(const auto object=constructed_object_owners_.find(source);object!=constructed_object_owners_.end()) {
    if(!object->second.auxiliary) object->second.auxiliary=std::make_unique<IntroOwnerAuxiliary>();
    return *object->second.auxiliary;
  }
  if(auto* group=group_owner(source_handle(source))) {
    if(!group->auxiliary_state) group->auxiliary_state=std::make_shared<IntroOwnerAuxiliary>();
    return *group->auxiliary_state;
  }
  throw std::runtime_error("Owner auxiliary factory is not admitted for this class");
}
void IntroRuntime::assign_directory_property(std::size_t source) {
  const auto offset=resources_.sources().directory().at(source).buf_auxiliary_offset;
  const auto names=resources_.source_names();
  if(!offset || offset>=names.size()) throw std::runtime_error("Owner property offset is outside retained BUF");
  assign_owner_property_data(source,names.subspan(offset));
}
void IntroRuntime::assign_owner_property_data(std::size_t source,std::span<const std::byte> section) {
  if(resource_load_stage_==IntroResourceLoadStage::failed || section.size()<8)
    throw std::runtime_error("Owner property section requires a complete header");
  std::uint32_t encoded=0;
  for(std::size_t i=0;i<4;++i) encoded|=std::uint32_t{std::to_integer<std::uint8_t>(section[4+i])}<<(i*8);
  const auto length=static_cast<std::size_t>(encoded&0x3fffffffU);
  if(length<8 || length>section.size()) throw std::runtime_error("Owner property section is truncated");
  const auto retained=resources_.source_names();
  const auto start=reinterpret_cast<std::uintptr_t>(retained.data());
  const auto address=reinterpret_cast<std::uintptr_t>(section.data());
  const bool borrowed=address>=start && address-start<retained.size();
  if(borrowed && length>retained.size()-(address-start))
    throw std::runtime_error("Borrowed owner property section exceeds retained allocation");
  auto& auxiliary=ensure_owner_auxiliary(source);
  if(borrowed) {
    auxiliary.borrowed_property_data=section.first(length);
    auxiliary.owned_property_data.clear();
  } else {
    std::vector<std::byte> copy(section.begin(),section.begin()+static_cast<std::ptrdiff_t>(length));
    auxiliary.owned_property_data=std::move(copy);
    auxiliary.borrowed_property_data={};
  }
}

IntroAuthoredGroupOwner* IntroRuntime::group_owner(IntroRuntimeHandle owner) {
  if(first_authored_group_ && first_authored_group_->owner==owner) return &*first_authored_group_;
  if(language_owner_ && language_owner_->owner==owner) return &*language_owner_;
  for(auto& [source,group]:constructed_group_owners_) {static_cast<void>(source);if(group.owner==owner) return &group;}
  for(auto& [source,room]:constructed_room_owners_) {static_cast<void>(source);if(room.group.owner==owner) return &room.group;}
  for(auto& [source,window]:window_owners_) {static_cast<void>(source);if(window->group.owner==owner) return &window->group;}
  return nullptr;
}
IntroConstructedRoomOwner* IntroRuntime::nearest_authored_room(IntroRuntimeHandle parent) {
  std::size_t steps=0;
  while(parent!=root_handle()) {
    for(auto& [source,room]:constructed_room_owners_) {static_cast<void>(source);if(room.group.owner==parent) return &room;}
    const auto next=resource_parent(parent);
    if(!next.value || ++steps>hierarchy_.size()) throw std::runtime_error("Nearest Room traversal is not rooted");
    parent=resource_owner(next);
  }
  return nullptr;
}
void IntroRuntime::attach_directory_owner(std::size_t source_index,IntroRuntimeResourceHandle resource,IntroRuntimeHandle parent) {
  const auto& source=resources_.sources().directory().at(source_index);
  const auto index=resource_index(resource);
  auto merged=resource_states_.at(index)->flags|(source.object_flags&0xfffffU);
  if(merged&0x8080U) merged|=0x8080U;
  if(merged&0x44000U) {
    if(saved_resource_flags_.size()>=1000) throw std::runtime_error("Saved resource flags capacity exhausted");
    saved_resource_flags_.push_back({resource,merged});
    merged&=~0x44000U;
  }
  set_resource_flags_no_maintenance(resource,merged,~merged,{resource_allocation_enabled_,false});
  hierarchy_[index].parent=resource_index(resource_handle(parent));
  auto* room=nearest_authored_room(parent);
  if(source.source_type==0x00100021U) {
    if(room) room->rooms.push_back(source_handle(source_index));
    else root_owner_state_->rooms.push_back(source_handle(source_index));
  }
  if(source.pool_class%8==2) {
    auto* direct=group_owner(parent);
    if(!direct) throw std::runtime_error("Category-two attachment requires its actual group");
    direct->category_two.push_back(resource);
    auto ancestor=parent;
    for(std::size_t steps=0;;++steps) {
      if(steps>hierarchy_.size()) throw std::runtime_error("Capability ancestry is cyclic");
      if(ancestor==root_handle()) {root_owner_state_->aggregate_flags|=0x10000U;break;}
      auto* group=group_owner(ancestor);
      if(!group) throw std::runtime_error("Capability ancestor is not an actual group owner");
      group->flags|=0x10000U;
      ancestor=resource_owner(resource_parent(ancestor));
    }
  }
  const auto current=resource_states_[index]->flags;
  set_resource_flags_no_maintenance(resource,current,~current,{resource_allocation_enabled_,false});
  if(!(current&0x400U) && source.pool_class%8==1 && room) {
    const auto found=constructed_object_owners_.find(source_index);
    if(found==constructed_object_owners_.end() || found->second.class_identifier!=0x00200002U || found->second.classification)
      throw std::runtime_error("Room membership requires the concrete ordinary-object classification");
    if(std::ranges::find(room->ordinary_members,resource)==room->ordinary_members.end()) room->ordinary_members.push_back(resource);
  }
}

void IntroRuntime::apply_directory_transform(std::size_t row,IntroRuntimeResourceHandle resource) {
  const auto index=resource_index(resource);
  const auto& source=resources_.sources().directory().at(row);
  auto& pose=hierarchy_.at(index);
  bool equal=true;
  for(std::size_t i=0;i<3;++i)
    if(std::bit_cast<std::uint32_t>(source.position[i])!=std::bit_cast<std::uint32_t>(pose.position[i])) equal=false;
  if(equal) for(std::size_t i=0;i<6;++i)
    if(std::bit_cast<std::uint32_t>(source.basis[i])!=std::bit_cast<std::uint32_t>(pose.matrix[i])) equal=false;
  if(equal) return;
  if(!directory_position_controls_prepared_ || position_mode_.immediate || position_mode_.collection_enabled || position_updates_.failed())
    throw std::runtime_error("Directory transform requires established disabled position collection");
  auto& state=resource_states_.at(index);
  if(!state || !resource_owners_.at(index)) throw std::runtime_error("Directory transform requires a canonical live resource");
  pose.position=source.position;
  state->flags|=0x00100000U;
  pose.matrix=source.basis;
  state->flags|=0x00100000U;
  // This concrete early branch does not obtain/retain a handle or read flags.
  position_updates_.notify_with_collection_disabled(position_mode_);
}

void IntroRuntime::construct_authored_camera_without_engine_renderer() {
  constexpr std::size_t row=5;
  if(resource_load_stage_!=IntroResourceLoadStage::picture_component_prefix_ready || !window_owner_ ||
      !language_owner_ || !live_cameras_.empty() || resource_allocation_enabled_ ||
      components_.construction_mode() || manager_row_edit_ || scene_resource_edit_ ||
      loaded_resource_handles_.size()!=row || count_group_selector_!=3 ||
      current_source_parent()!=window_owner_->group.owner)
    throw std::runtime_error("Authored camera requires the completed Picture prefix");
  if(!application_.has_class_registration(0x00400003U))
    throw std::runtime_error("Authored camera requires its concrete class registration");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<=row || resources_.camera_index()!=row)
    throw std::runtime_error("Authored camera is not the next directory source");
  const auto& source=directory[row];
  const auto zero=[](float value){return value==0.0F && !std::signbit(value);};
  const auto identity=[](const std::array<float,9>& basis) {
    for(std::size_t i=0;i<basis.size();++i)
      if(std::bit_cast<std::uint32_t>(basis[i])!=std::bit_cast<std::uint32_t>(engine_identity[i])) return false;
    return true;
  };
  if(source.source_type!=0x00400003U || source.source_variant || source.parent_steps ||
      source.enters_child_pool || source.object_flags!=0x00200400U || source.class_data_value ||
      source.auxiliary_value || source.buf_auxiliary_offset || source.child_value ||
      source.post_load_source_offset || !source.deferred_source_offset || !source.attachments.empty() ||
      !identity(source.basis) || !std::ranges::all_of(source.position,zero) ||
      source.pool_class!=3 || source.pool_group!=2 || !owner_components(source_handle(row)).empty())
    throw std::runtime_error("Unsupported authored camera source shape");
  const auto* fade=constructed_picture_owner(4);
  const auto& root_state=resource_state(root_handle());
  const auto parent=window_owner_->group.owner;
  const auto parent_resource=resource_handle(parent);
  const auto& parent_state=resource_state_for_handle(parent_resource);
  if(!fade || !root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
      root_owner_state_->aggregate_flags || !root_owner_state_->category_memberships.empty() ||
      !parent_state || parent_state->flags!=0x09000000U || parent_state->context.value ||
      window_owner_->group.aggregate_flags || resource_parent(parent)!=resource_handle(root_handle()) ||
      child_owners(parent)!=std::vector<IntroRuntimeHandle>{language_owner_->owner,fade->owner})
    throw std::runtime_error("Authored camera requires the retained fresh Window ancestry");
  try {
    advance_source_loading_progress_without_engine_renderer(row);
    const auto scope_it=std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group);
    if(scope_it==source_resource_scopes_.end()) throw std::runtime_error("Camera allocation scope is absent");
    auto& scope=*scope_it;
    auto& cursor=scope.next_in_partition[3];
    const auto begin=std::uint64_t{scope.counts[0]}+scope.counts[1]+scope.counts[2];
    const auto end=begin+scope.counts[3];
    if(!cursor || *cursor<begin || *cursor>=end)
      throw std::runtime_error("Camera partition is absent or exhausted");
    const auto resource=scope.resources.at(*cursor);
    const auto index=resource_index(resource);
    const auto owner=source_handle(row);
    if(index!=hierarchy_index(owner) || resource_owners_[index] || !resource_states_[index] ||
        resource_states_[index]->flags!=0x09000000U || resource_states_[index]->context.value ||
        resource_states_[index]->metadata || resource_states_[index]->directory_auxiliary ||
        hierarchy_[index].parent!=no_picture_transform_parent || !identity(hierarchy_[index].matrix) ||
        !std::ranges::all_of(hierarchy_[index].position,zero))
      throw std::runtime_error("Camera requires its fresh supplied resource");
    const auto names=resources_.source_names();
    if(source.buf_name_offset>=names.size()) throw std::runtime_error("Camera name is out of range");
    std::size_t name_end=source.buf_name_offset;
    while(name_end<names.size() && names[name_end]!=std::byte{0}) ++name_end;
    if(name_end==names.size()) throw std::runtime_error("Camera name is unterminated");
    ++*cursor;
    std::string name;
    for(std::size_t i=source.buf_name_offset;i<name_end;++i) name.push_back(static_cast<char>(names[i]));
    auto live=std::make_unique<IntroLiveCameraOwner>();
    live->metadata={owner,resource,std::move(name)};
    live->context=root_handle();
    auto& camera=*live_cameras_.emplace(row,std::move(live)).first->second;
    resource_owners_[index]=owner;
    camera.metadata.notification_sequence=application_.register_class_instance(source.source_type);
    manager_row_edit_=true;scene_resource_edit_=true;
    loaded_resource_handles_.push_back(resource);
    resource_states_[index]->metadata=0;resource_states_[index]->directory_auxiliary=0;
    // The validated authored pose equals the fresh pose bitwise: the real
    // transform setter's equality return performs no dirtying or queue work.
    const auto merged=resource_states_[index]->flags|(source.object_flags&0xfffffU);
    set_resource_flags_no_maintenance(resource,merged,~merged,{resource_allocation_enabled_,false});
    hierarchy_[index].parent=resource_index(parent_resource);
    const auto current=resource_states_[index]->flags;
    set_resource_flags_no_maintenance(resource,current,~current,{resource_allocation_enabled_,false});
    // Normal registration returns at resource hide. Camera enabled is a
    // separate word; neither renderer membership nor dimensions change here.
    deferred_reader_work_.push_back({resource,source.deferred_source_offset,row,false,{}});
    directory_resource_mapping_.at(row)=resource;
    manager_row_edit_=false;scene_resource_edit_=false;
    resource_load_stage_=IntroResourceLoadStage::authored_camera_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_second_window_picture_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::authored_camera_ready || !window_owner_ || !language_owner_ ||
      !constructed_camera_owner() || resource_allocation_enabled_ || components_.construction_mode() ||
      manager_row_edit_ || scene_resource_edit_ || loaded_resource_handles_.size()!=6 ||
      count_group_selector_!=3 || current_source_parent()!=window_owner_->group.owner)
    throw std::runtime_error("Second Window requires the completed authored camera stage");
  if(!application_.has_class_registration(0x00100030U) || !application_.has_class_registration(0x00200046U) ||
      !application_.has_component_class_registration("ZWINPIC_FadeToBlack"))
    throw std::runtime_error("Second Window requires concrete Window, Picture and Fade registrations");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<8) throw std::runtime_error("Second Window sources are absent");
  const auto zero=[](float value){return std::bit_cast<std::uint32_t>(value)==0U;};
  const auto identity=[](const auto& matrix) {
    for(std::size_t i=0;i<engine_identity.size();++i)
      if(std::bit_cast<std::uint32_t>(matrix[i])!=std::bit_cast<std::uint32_t>(engine_identity[i])) return false;
    return true;
  };
  for(std::size_t row=6;row<=7;++row) {
    const auto& source=directory[row];
    const bool window=row==6;
    if(source.source_type!=(window?0x00100030U:0x00200046U) || source.source_variant ||
        source.parent_steps!=(window?1:0) || source.enters_child_pool!=window ||
        source.object_flags!=(window?0x03000000U:0x00200000U) || source.class_data_value ||
        source.auxiliary_value || source.buf_auxiliary_offset || source.child_value ||
        source.post_load_source_offset || !source.deferred_source_offset ||
        !identity(source.basis) || !std::ranges::all_of(source.position,zero) ||
        source.pool_class!=(window?0U:1U) || source.pool_group!=(window?0U:4U))
      throw std::runtime_error("Unsupported second Window source shape");
    if(window) {
      if(source.attachment_table_offset || !source.attachments.empty() || !owner_components(source_handle(row)).empty())
        throw std::runtime_error("Second Window has unsupported attachments");
    } else if(source.attachments.size()!=1 || resources_.sources().attachment_identifier(row,0)!="ZWINPIC_FadeToBlack" ||
        source.attachments.front().parameter!=0.0F || owner_components(source_handle(row)).size()!=1) {
      throw std::runtime_error("Second Window Picture requires its actual Fade attachment");
    }
  }
  const auto& root_state=resource_state(root_handle());
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
      root_owner_state_->aggregate_flags || !root_owner_state_->category_memberships.empty() ||
      hierarchy_[0].parent!=no_picture_transform_parent || !identity(hierarchy_[0].matrix) ||
      !std::ranges::all_of(hierarchy_[0].position,zero) ||
      resource_parent(window_owner_->group.owner)!=resource_handle(root_handle()) ||
      child_owners(root_handle())!=std::vector<IntroRuntimeHandle>{source_handle(0),window_owner_->group.owner})
    throw std::runtime_error("Second Window requires the retained fresh ROOT ancestry");
  try {
    advance_source_loading_progress_without_engine_renderer(6);
    current_source_parent_=root_handle();
    construct_group_row_without_engine_renderer(6);
    advance_source_loading_progress_without_engine_renderer(7);
    allocate_source_scope(count_group_selector_);
    construct_picture_row_without_engine_renderer(7);
    resource_load_stage_=IntroResourceLoadStage::second_window_picture_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

const IntroConstructedCameraOwner* IntroRuntime::constructed_camera_owner(std::size_t source) const noexcept {
  const auto found=live_cameras_.find(source);
  return found==live_cameras_.end()?nullptr:&found->second->metadata;
}
const IntroConstructedCharacterOwner* IntroRuntime::constructed_character_owner(std::size_t source) const noexcept {
  const auto found=constructed_character_owners_.find(source);
  return found==constructed_character_owners_.end()?nullptr:&found->second;
}
const IntroConstructedListOwner* IntroRuntime::constructed_list_owner(std::size_t source) const noexcept {
  const auto found=constructed_list_owners_.find(source);
  return found==constructed_list_owners_.end()?nullptr:&found->second;
}
const IntroConstructedPictureComponent* IntroRuntime::constructed_attachment(std::size_t component) const noexcept {
  const auto found=constructed_picture_components_.find(component);
  return found==constructed_picture_components_.end()?nullptr:&found->second;
}
const IntroConstructedPictureOwner* IntroRuntime::constructed_visual_owner(std::size_t source) const noexcept {
  const auto found=constructed_visual_owners_.find(source);
  return found==constructed_visual_owners_.end()?nullptr:&found->second;
}
const IntroAuthoredGroupOwner* IntroRuntime::constructed_group_owner(std::size_t source) const noexcept {
  if(source==0) return first_authored_group_?&*first_authored_group_:nullptr;
  if(language_owner_ && source<resources_.sources().directory().size() && source_handle(source)==language_owner_->owner) return &*language_owner_;
  if(const auto window=window_owners_.find(source);window!=window_owners_.end()) return &window->second->group;
  if(const auto room=constructed_room_owners_.find(source);room!=constructed_room_owners_.end()) return &room->second.group;
  const auto found=constructed_group_owners_.find(source);
  return found==constructed_group_owners_.end()?nullptr:&found->second;
}
const IntroConstructedRoomOwner* IntroRuntime::constructed_room_owner(std::size_t source) const noexcept {
  const auto found=constructed_room_owners_.find(source);
  return found==constructed_room_owners_.end()?nullptr:&found->second;
}
const IntroConstructedObjectOwner* IntroRuntime::constructed_object_owner(std::size_t source) const noexcept {
  const auto found=constructed_object_owners_.find(source);
  return found==constructed_object_owners_.end()?nullptr:&found->second;
}
const IntroOwnerAuxiliary* IntroRuntime::constructed_owner_auxiliary(std::size_t source) const noexcept {
  if(const auto found=live_cameras_.find(source);found!=live_cameras_.end()) return found->second->auxiliary.get();
  if(const auto found=constructed_object_owners_.find(source);found!=constructed_object_owners_.end()) return found->second.auxiliary.get();
  if(const auto* group=constructed_group_owner(source)) return group->auxiliary_state.get();
  return nullptr;
}

void IntroRuntime::prepare_scene_lifetime_keys_registry() {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete)
    throw std::runtime_error("Scene KEYS registry requires complete directory construction");
  if(scene_lifetime_keys_registry_)
    throw std::runtime_error("Scene KEYS registry is already prepared");

  const auto& directory=resources_.sources().directory();
  const auto retained=resources_.source_names();
  std::vector<data::OwnerAuxiliaryPropertyBlock> properties;
  properties.reserve(directory.size());

  // Preflight the entire candidate set before allocating/publishing the
  // registry. A MatPosAnim attachment is the sole admission route; a generic
  // BUF owner is never promoted just because it happens to resemble KEYS.
  for(std::size_t row=0;row<directory.size();++row) {
    const auto& source=directory[row];
    bool has_mat_pos_anim=false;
    for(std::size_t slot=0;slot<source.attachments.size();++slot) {
      if(resources_.sources().attachment_identifier(row,slot)=="ZGEOM_MatPosAnim") {
        if(has_mat_pos_anim)
          throw std::runtime_error("Scene KEYS registry rejects multiple MatPosAnim attachments for one owner");
        has_mat_pos_anim=true;
      }
    }
    if(!has_mat_pos_anim) continue;
    if(source.buf_auxiliary_offset==0U || source.buf_auxiliary_offset>=retained.size())
      throw std::runtime_error("MatPosAnim owner lacks a canonical BUF property offset");
    const auto owner=source_handle(row);
    if(!live_owner(owner.value))
      throw std::runtime_error("MatPosAnim owner is not live during KEYS preparation");
    const auto* auxiliary=constructed_owner_auxiliary(row);
    const auto offset=static_cast<std::size_t>(source.buf_auxiliary_offset);
    if(!auxiliary || !auxiliary->owned_property_data.empty() || auxiliary->borrowed_property_data.size()!=64U ||
        auxiliary->borrowed_property_data.data()!=retained.data()+offset)
      throw std::runtime_error("MatPosAnim owner lacks its canonical retained BUF property view");
    const data::OwnerAuxiliaryPropertyBlock property{
        owner.value,source.buf_auxiliary_offset,auxiliary->borrowed_property_data};
    // Parser validation occurs before any opaque identity is reserved.
    static_cast<void>(data::OwnerBufKeysProfileParser::parse(property));
    properties.push_back(property);
  }
  if(properties.empty())
    throw std::runtime_error("Scene KEYS registry found no supported MatPosAnim owners");
  if(properties.size()>std::numeric_limits<std::uint64_t>::max()-next_scene_lifetime_keys_handle_+1U)
    throw std::runtime_error("Scene KEYS registry opaque handle range is exhausted");

  std::vector<data::SceneLifetimeKeysRegistryInput> inputs;
  inputs.reserve(properties.size());
  std::uint64_t staged_next=next_scene_lifetime_keys_handle_;
  for(const auto& property:properties) {
    if(staged_next==0U)
      throw std::runtime_error("Scene KEYS registry opaque handle range is exhausted");
    inputs.push_back({property,staged_next++});
  }

  auto staged=data::SceneLifetimeKeysRegistry::construct(inputs);
  for(const auto& input:inputs) {
    const auto child=data::OwnerBufKeysProfileParser::parse(input.property);
    const std::array children{child};
    const auto materialized=data::KeysPropertyMaterializer::materialize(
        input.property,children,staged.materializer_resolver());
    if(!materialized || materialized->owner!=input.property.owner ||
        materialized->buf_auxiliary_offset!=input.property.buf_auxiliary_offset ||
        materialized->opaque_handle!=input.opaque_handle)
      throw std::runtime_error("Scene KEYS registry materialization did not preserve the canonical owner join");
  }

  scene_lifetime_keys_registry_.emplace(std::move(staged));
  next_scene_lifetime_keys_handle_=staged_next;
}

void IntroRuntime::prepare_scene_lifetime_keys_backing() {
  if (!scene_lifetime_keys_registry_)
    throw std::runtime_error("Scene KEYS backing requires the prepared owner-local registry");
  if (scene_lifetime_keys_backing_)
    throw std::runtime_error("Scene KEYS backing is already prepared");
  if (next_scene_lifetime_keys_backing_generation_ == 0U)
    throw std::runtime_error("Scene KEYS backing generation is exhausted");

  const auto retained = resources_.source_names();
  const auto staged_backing = data::ImmutableKeysBackingView::create(
      next_scene_lifetime_keys_backing_generation_, retained);
  if (!staged_backing)
    throw std::runtime_error("Scene KEYS backing requires the retained FF-Intro BUF allocation");

  std::vector<SceneLifetimeKeysBackingBinding> staged_bindings;
  const auto& directory = resources_.sources().directory();
  staged_bindings.reserve(scene_lifetime_keys_registry_->size());
  for (std::size_t row = 0; row < directory.size(); ++row) {
    const auto& source = directory[row];
    bool has_mat_pos_anim = false;
    for (std::size_t slot = 0; slot < source.attachments.size(); ++slot) {
      if (resources_.sources().attachment_identifier(row, slot) == "ZGEOM_MatPosAnim") {
        if (has_mat_pos_anim)
          throw std::runtime_error("Scene KEYS backing rejects multiple MatPosAnim attachments for one owner");
        has_mat_pos_anim = true;
      }
    }
    if (!has_mat_pos_anim)
      continue;

    const auto owner = source_handle(row);
    const auto* auxiliary = constructed_owner_auxiliary(row);
    const auto offset = static_cast<std::size_t>(source.buf_auxiliary_offset);
    if (owner.value == 0U || source.buf_auxiliary_offset == 0U || offset >= retained.size() ||
        !auxiliary || !auxiliary->owned_property_data.empty() || auxiliary->borrowed_property_data.size() != 64U ||
        auxiliary->borrowed_property_data.data() != retained.data() + offset) {
      throw std::runtime_error("Scene KEYS backing requires canonical retained MatPos owner properties");
    }

    const data::OwnerAuxiliaryPropertyBlock property{
        owner.value, source.buf_auxiliary_offset, auxiliary->borrowed_property_data};
    const auto child = data::OwnerBufKeysProfileParser::parse(property);
    const auto resolved = scene_lifetime_keys_registry_->resolve(property, child);
    if (!resolved || resolved->owner != owner.value ||
        resolved->buf_auxiliary_offset != source.buf_auxiliary_offset || resolved->opaque_handle == 0U) {
      throw std::runtime_error("Scene KEYS backing could not resolve the canonical owner-local child");
    }
    const data::MaterializedOwnerKeysChild materialized{
        resolved->owner, resolved->buf_auxiliary_offset, resolved->opaque_handle};
    const auto bound = data::KeysDescriptorRangeBinder::bind(
        materialized, child, [&staged_backing](const data::KeysDescriptorBackingRequest&) {
          return !staged_backing->bytes().empty();
        });
    if (!bound)
      throw std::runtime_error("Scene KEYS backing rejected an unavailable descriptor range");

    // The evaluator is the recovered read-only consumer of this allocation.
    // Probe both inclusive endpoints before publication so an apparent range
    // cannot become a live binding when either stream/table endpoint is bad.
    if (!data::KeysBackingEvaluator::evaluate(*staged_backing, *bound, 0.0F) ||
        !data::KeysBackingEvaluator::evaluate(*staged_backing, *bound, 1.0F)) {
      throw std::runtime_error("Scene KEYS backing rejected an unreadable descriptor range");
    }
    staged_bindings.push_back({owner.value, resolved->opaque_handle, *bound});
  }
  if (staged_bindings.size() != scene_lifetime_keys_registry_->size())
    throw std::runtime_error("Scene KEYS backing did not bind every prepared MatPos owner");

  scene_lifetime_keys_backing_.emplace(*staged_backing);
  scene_lifetime_keys_backing_bindings_ = std::move(staged_bindings);
  ++next_scene_lifetime_keys_backing_generation_;
}

std::optional<data::KeysBackingSample> IntroRuntime::evaluate_scene_lifetime_keys(
    IntroRuntimeHandle owner, float normalized_coordinate) const noexcept {
  if (!scene_lifetime_keys_backing_ || owner.value == 0U)
    return std::nullopt;
  const auto binding = std::find_if(scene_lifetime_keys_backing_bindings_.begin(),
                                    scene_lifetime_keys_backing_bindings_.end(),
                                    [owner](const SceneLifetimeKeysBackingBinding& candidate) {
                                      return candidate.owner == owner.value;
                                    });
  if (binding == scene_lifetime_keys_backing_bindings_.end())
    return std::nullopt;
  return data::KeysBackingEvaluator::evaluate(*scene_lifetime_keys_backing_, binding->descriptor,
                                              normalized_coordinate);
}

void IntroRuntime::construct_lens_flare_animation_scope_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::room_animation_scope_ready || loaded_resource_handles_.size()!=69 ||
      count_group_selector_!=9 || current_source_parent()!=source_handle(48) || resource_allocation_enabled_ ||
      components_.construction_mode() || manager_row_edit_ || scene_resource_edit_ || !directory_position_controls_prepared_ ||
      position_mode_.immediate || position_mode_.collection_enabled || position_updates_.failed())
    throw std::runtime_error("Lens flare animation scope requires completed Room construction and retained loader controls");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<200) throw std::runtime_error("Lens flare animation sources are absent");
  const auto expected_type=[](std::size_t row)->std::uint32_t {
    if(row==69 || row==100 || row==108 || row==109 || row==124) return 0x00100001U;
    if(row==70) return 0x00100030U;
    if(row==71) return 0x0010002eU;
    if(row==72) return 0x00100031U;
    if(row<=97) return 0x00200046U;
    if(row==98 || row==179) return 0x00400003U;
    if(row==99) return 0x0080000dU;
    if(row>=101 && row<=107) return 0x002000e5U;
    if(row==110 || row==111) return 0x0020000bU;
    if(row==140 || row==142) return 0x00800023U;
    if(row==141 || row==157 || row==162 || row==194 || row==196) return 0x00800024U;
    if(row==178) return 0x04000022U;
    if(row>=197) return 0x0800001aU;
    return 0x00200002U;
  };
  for(std::size_t row=69;row<=199;++row) {
    const auto& source=directory[row];
    const auto type=expected_type(row);
    const bool group=type==0x00100001U || type==0x00100030U || type==0x0010002eU || type==0x00100031U;
    const auto pop=row==100 || row==124?2U:row==69 || row==98 || row==108 || row==197?1U:0U;
    const auto bank=row==110 || row==111?1U:row==154 || row==158?2U:0U;
    const auto category=group?0U:type==0x0080000dU || type==0x00800023U || type==0x00800024U?2U:
        type==0x00400003U || type==0x0800001aU?3U:1U;
    const bool property=row==111 || row==157 || row==160 || row==161 || (row>=169 && row<=177) ||
        (row>=179 && row<=190) || row==192 || row==193;
    const bool no_reader=row==112 || row==113 || (row>=115 && row<=123) || (row>=143 && row<=153) ||
        row==155 || row==156 || row==159 || row==163 || (row>=165 && row<=168) || row==195;
    if(source.source_type!=type || !application_.has_class_registration(type) || source.parent_steps!=pop ||
        source.enters_child_pool!=group || source.pool_class!=bank*8+category || source.source_variant!=bank ||
        source.post_load_source_offset || owner_components(source_handle(row)).size()!=source.attachments.size() ||
        (source.buf_auxiliary_offset!=0)!=property || (source.deferred_source_offset!=0)==no_reader ||
        !std::ranges::all_of(source.position,[](float value){return std::isfinite(value);}) ||
        !std::ranges::all_of(source.basis,[](float value){return std::isfinite(value);}))
      throw std::runtime_error("Unsupported lens flare animation source shape");
    std::vector<std::pair<std::string_view,std::uint32_t>> expected;
    if(row==70) expected.emplace_back("ZGEOM_FilmGrainCamSetup",0);
    if(row==72) expected.emplace_back("ZWINDOW_LensFlareControl",0);
    if(row>=73 && row<=97) expected.emplace_back("ZWINPIC_LensFlare",0);
    if(row==114 || (row>=125 && row<=139) || row==154 || row==158 || row==191)
      expected.emplace_back("ZSTDOBJ_VertAnim",0x3f800000U);
    if(row==111 || row==160 || row==161 || (row>=169 && row<=177) || (row>=179 && row<=190) || row==192 || row==193)
      expected.emplace_back("ZGEOM_MatPosAnim",row==179?0x42c80000U:0x3f800000U);
    if(row==157) expected.emplace_back("ZGEOM_ParamAnim",0x3f800000U);
    if(row==164 || row==169 || row==172 || row==180) expected.emplace_back("ZGEOM_ParticleEmitter",0x3f800000U);
    if(row==197) expected.emplace_back("ZLIST_CutSequence",0);
    if(row==198) expected.emplace_back("ZLIST_CutSequenceList",0);
    if(row==199) expected.emplace_back("ZLIST_LensFlareLights",0);
    if(source.attachments.size()!=expected.size() || (expected.empty() && source.attachment_table_offset))
      throw std::runtime_error("Unsupported lens flare animation attachment count");
    if(!expected.empty()) {
      const bool hidden=(row>=125 && row<=139) || row==154 || row==157 || row==158 || row==160 || row==161 ||
          row==164 || (row>=169 && row<=193);
      if(((source.object_flags&0x400U)!=0)!=hidden)
        throw std::runtime_error("Unsupported lens flare animation owner hide state");
    }
    for(std::size_t slot=0;slot<expected.size();++slot) {
      const auto factory=resources_.sources().attachment_identifier(row,slot);
      if(factory!=expected[slot].first || !application_.has_component_class_registration(factory) ||
          std::bit_cast<std::uint32_t>(source.attachments[slot].parameter)!=expected[slot].second)
        throw std::runtime_error("Unsupported lens flare animation attachment factory or raw argument");
    }
  }
  try {
    for(std::size_t row=69;row<=199;++row) {
      const auto& source=directory[row];
      advance_source_loading_progress_without_engine_renderer(row);
      for(std::size_t pop=0;pop<source.parent_steps;++pop) {
        const auto parent=resource_parent(current_source_parent());
        if(!parent.value) throw std::runtime_error("Directory scope pop exceeds ROOT");
        current_source_parent_=resource_owner(parent);
      }
      if(std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group)==source_resource_scopes_.end()) {
        if(source.pool_group!=count_group_selector_) throw std::runtime_error("Directory scope allocation selector mismatch");
        allocate_source_scope(source.pool_group);
      }
      if(source.pool_class%8==0) construct_group_row_without_engine_renderer(row);
      else construct_non_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::lens_flare_animation_scope_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_remaining_directory_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::lens_flare_animation_scope_ready || loaded_resource_handles_.size()!=200 ||
      count_group_selector_!=17 || current_source_parent()!=source_handle(69) || resource_allocation_enabled_ ||
      components_.construction_mode() || manager_row_edit_ || scene_resource_edit_ || !directory_position_controls_prepared_ ||
      position_mode_.immediate || position_mode_.collection_enabled || position_updates_.failed() ||
      sound_load_policy_!=IntroSoundLoadPolicy::directory_construction || !source_script_work_.empty())
    throw std::runtime_error("Remaining directory construction requires completed lens flare scope and retained loader controls");
  const auto& directory=resources_.sources().directory();
  if(directory.size()!=470) throw std::runtime_error("Remaining directory construction requires the complete supported source population");
  const auto between=[](std::size_t row,std::size_t first,std::size_t last){return row>=first && row<=last;};
  constexpr std::array<std::size_t,11> groups{200,201,284,285,291,292,399,400,406,407,422};
  constexpr std::array<std::size_t,12> pops{200,282,284,291,371,396,399,406,422,438,457,459};
  constexpr std::array<std::size_t,10> lights{215,223,276,388,389,393,394,454,455,456};
  for(std::size_t row=200;row<=469;++row) {
    const auto& source=directory[row];
    const bool group=std::ranges::find(groups,row)!=groups.end();
    const bool light=std::ranges::find(lights,row)!=lights.end();
    const bool camera=row==220 || row==374 || row==448 || row==461;
    const bool list=row==282 || row==283 || row==397 || row==398 || between(row,457,460) || between(row,463,466);
    const bool particle=between(row,286,290) || between(row,401,405);
    const bool linked_light=row==277 || row==387 || row==396;
    const bool sound_light=row==373 || row==380 || row==381;
    const auto type=row==201?0x00100021U:group?0x00100001U:camera?0x00400003U:list?0x0800001aU:
        particle?0x002000e5U:light?0x00800024U:linked_light?0x0080000dU:row==278?0x00800023U:
        sound_light?0x00800020U:row==462?0x08000049U:row==467 || row==468?0x00200012U:row==469?0x002000e4U:0x00200002U;
    const auto category=group?0U:light || linked_light || sound_light || row==278?2U:
        camera || list || row==462 || row==467 || row==468?3U:1U;
    const auto bank=row==215 || row==223?2U:0U;
    const auto pop=std::ranges::find(pops,row)!=pops.end()?1U:0U;
    const bool no_reader=between(row,202,214) || row==224 || row==241 || row==447;
    const bool property=between(row,216,223) || between(row,225,240) || between(row,243,275) || between(row,278,281) ||
        between(row,293,386) || between(row,388,395) || between(row,408,421) || between(row,423,446) || between(row,448,456);
    const bool hidden=between(row,215,223) || between(row,225,240) || between(row,243,276) || between(row,278,281) ||
        between(row,293,386) || between(row,388,392) || row==395 || between(row,408,421) || between(row,423,446) || between(row,448,455);
    if(source.source_type!=type || !application_.has_class_registration(type) || source.enters_child_pool!=group ||
        source.parent_steps!=pop || source.pool_class!=bank*8+category || source.source_variant!=bank || source.post_load_source_offset ||
        ((source.object_flags&0x400U)!=0)!=hidden || (source.buf_auxiliary_offset!=0)!=property ||
        (source.deferred_source_offset!=0)==no_reader ||
        ((source.object_flags&0x44000U)!=0)!=(bank==2) ||
        owner_components(source_handle(row)).size()!=source.attachments.size() ||
        !std::ranges::all_of(source.position,[](float value){return std::isfinite(value);}) ||
        !std::ranges::all_of(source.basis,[](float value){return std::isfinite(value);}))
      throw std::runtime_error("Unsupported remaining directory source shape");
    std::vector<std::pair<std::string_view,std::uint32_t>> expected;
    const bool mat=between(row,216,222) || between(row,225,240) || between(row,243,275) || between(row,279,281) ||
        between(row,293,372) || between(row,374,379) || between(row,382,386) || between(row,390,392) || row==395 ||
        between(row,408,421) || between(row,423,446) || between(row,448,453);
    if(mat) expected.emplace_back("ZGEOM_MatPosAnim",row==220 || row==374 || row==448?0x42c80000U:0x3f800000U);
    if(row==215 || row==223 || row==278 || row==373 || row==380 || row==381 || row==388 || row==389 || row==454 || row==455)
      expected.emplace_back("ZGEOM_ParamAnim",0x3f800000U);
    if(row==219 || row==242 || row==371 || row==377 || row==440) expected.emplace_back("ZSTDOBJ_ScrollTexture",0);
    if(row==222 || row==451) expected.emplace_back("ZSTDOBJ_VertAnim",0x3f800000U);
    if(row==282 || row==397 || row==457 || row==459) expected.emplace_back("ZLIST_CutSequence",row==282 || row==459?0x3f800000U:0);
    if(row==283 || row==398 || row==458 || row==460) expected.emplace_back("ZLIST_CutSequenceList",0);
    if(row==386 || row==392 || row==395 || row==406 || row==443 || row==444) expected.emplace_back("ZGEOM_ParticleEmitter",0x3f800000U);
    if(row==460) for(std::size_t command=0;command<5;++command) expected.emplace_back("ZLIST_CutSequenceCommand",0x3f800000U);
    if(row==465) expected.emplace_back("ZGEOM_MovieControl",0);
    if(row==466) for(std::size_t command=0;command<2;++command) expected.emplace_back("ZLIST_ExternCutSequenceCommand",0x3f800000U);
    if(row==467 || row==468) {
      expected.emplace_back("ZSNDOBJ_SoundExtend",0);
      expected.emplace_back("ZSNDOBJ_SoundNotify",0);
      expected.emplace_back("ZSNDOBJ_SoundSegment",0x3f800000U);
      expected.emplace_back("ZGEOM_ZSetZDefine",0);
    }
    if(source.attachments.size()!=expected.size() || (expected.empty() && source.attachment_table_offset))
      throw std::runtime_error("Unsupported remaining directory attachment count");
    for(std::size_t slot=0;slot<expected.size();++slot) {
      const auto factory=resources_.sources().attachment_identifier(row,slot);
      if(factory!=expected[slot].first || !application_.has_component_class_registration(factory) ||
          std::bit_cast<std::uint32_t>(source.attachments[slot].parameter)!=expected[slot].second)
        throw std::runtime_error("Unsupported remaining directory attachment factory or raw argument");
    }
  }
  try {
    for(std::size_t row=200;row<=469;++row) {
      const auto& source=directory[row];
      advance_source_loading_progress_without_engine_renderer(row);
      if(source.parent_steps) {
        const auto parent=resource_parent(current_source_parent());
        if(!parent.value) throw std::runtime_error("Directory scope pop exceeds ROOT");
        current_source_parent_=resource_owner(parent);
      }
      if(std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group)==source_resource_scopes_.end()) {
        if(source.pool_group!=count_group_selector_) throw std::runtime_error("Directory scope allocation selector mismatch");
        allocate_source_scope(source.pool_group);
      }
      if(source.pool_class%8==0) construct_group_row_without_engine_renderer(row);
      else construct_non_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::directory_construction_complete;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::prepare_deferred_references(IntroDeferredReaderWork& work) {
  const auto& sources=resources_.sources();
  const auto& directory=sources.directory();
  if(work.source_directory_index>=directory.size() ||
      directory.at(work.source_directory_index).deferred_source_offset!=work.source_offset)
    throw std::runtime_error("Deferred reader work has no matching source-directory block");

  // This marker belongs to the deferred block, rather than to an owner class
  // or allocation slot.  It is intentionally set before any concrete reader.
  work.processed=true;

  // The approved parser currently exposes authored object-reference lists only
  // for the controller's two explicit list sources.  Do not guess at another
  // tagged grammar merely because a later concrete reader has a source block.
  const auto sequence_source=sources.local_source_for_authored_reference(
      resources_.controller().sequence_reference);
  const auto group_source=sources.local_source_for_authored_reference(
      resources_.controller().group_reference);
  std::span<const std::uint32_t> raw_references;
  if(sequence_source && *sequence_source==work.source_directory_index)
    raw_references=resources_.cut_references();
  else if(group_source && *group_source==work.source_directory_index)
    raw_references=resources_.group_references();
  else
    return;

  std::vector<std::optional<IntroRuntimeResourceHandle>> translated;
  translated.reserve(raw_references.size());
  for(const auto raw_reference:raw_references) {
    const auto source=sources.local_source_for_authored_reference(raw_reference);
    if(!source) {
      translated.push_back(std::nullopt);
      continue;
    }
    if(*source>=directory_resource_mapping_.size() ||
        !directory_resource_mapping_.at(*source))
      throw std::runtime_error("Authored deferred reference has no runtime source-directory mapping");
    translated.push_back(*directory_resource_mapping_.at(*source));
  }
  work.translated_references=std::move(translated);
}

IntroLifecyclePreflightReport IntroRuntime::preflight_global_lifecycle() const {
  IntroLifecyclePreflightReport report;
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete) {
    report.failure=IntroLifecyclePreflightFailure::stage;
    return report;
  }
  IntroLifecycleAdmissionRequirements requirements;
  requirements.readers.reserve(deferred_reader_work_.size());
  for(const auto& work:deferred_reader_work_)
    requirements.readers.push_back({work.resource.value,work.source_offset,work.source_directory_index});
  requirements.owners.reserve(1U+loaded_resource_handles_.size());
  requirements.owners.push_back({root_handle().value});
  for(const auto resource:loaded_resource_handles_) {
    const auto owner=associated_resource_owner(resource);
    if(!owner) {
      report.failure=IntroLifecyclePreflightFailure::live_mapping;
      return report;
    }
    requirements.owners.push_back({owner->value});
  }
  report.expected_readers=requirements.readers.size();
  report.expected_owners=requirements.owners.size();
  for(const auto component_index:components_.construction_order()) {
    const auto& component=components_.at(component_index);
    if(component.source().synthesized) continue;
    requirements.components.push_back({component_handle(component_index)});
    ++report.expected_components;
    if(!component.constructed() || component.removed()) {
      report.failure=IntroLifecyclePreflightFailure::component_coverage;
      return report;
    }
  }
  if(loaded_resource_handles_.size()!=resources_.sources().directory().size() ||
      directory_resource_mapping_.size()!=resources_.sources().directory().size()) {
    report.failure=IntroLifecyclePreflightFailure::live_mapping;
    return report;
  }
  for(std::size_t source=0;source<directory_resource_mapping_.size();++source) {
    const auto resource=directory_resource_mapping_[source];
    if(!resource || !associated_resource_owner(*resource) ||
        *associated_resource_owner(*resource)!=source_handle(source)) {
      report.failure=IntroLifecyclePreflightFailure::live_mapping;
      return report;
    }
  }
  IntroLifecycleAdmissionCoverageRegistry admissions{std::move(requirements)};
  for(const auto identity:supported_reader_admissions_) admissions.cover_reader(identity);
  for(const auto identity:supported_component_admissions_) {
    const auto found=std::ranges::find_if(components_.construction_order(),[&](const auto index) {
      return component_handle(index)==identity.component;
    });
    if(found==components_.construction_order().end()) continue;
    const auto& component=components_.at(*found);
    const auto owner=IntroRuntimeHandle{component.state().attached_owner};
    const auto sound=std::ranges::find_if(sounds_,[&](const auto& candidate) {
      return candidate->handle()==owner;
    });
    if(component.removed() || !(component.state().status&4U) || sound==sounds_.end() ||
        !(*sound)->active() || (*sound)->failed() || !(*sound)->owner_binding())
      continue;
    admissions.cover_component(identity);
  }
  for(const auto identity:supported_owner_admissions_) {
    const auto sound=std::ranges::find_if(sounds_,[&](const auto& candidate) {
      return candidate->handle().value==identity.owner;
    });
    if(sound==sounds_.end() || !(*sound)->active() || (*sound)->failed() || !(*sound)->owner_binding())
      continue;
    admissions.cover_owner(identity);
  }
  const auto admitted=admissions.report();
  report.covered_readers=admitted.covered_readers;
  report.covered_components=admitted.covered_components;
  report.covered_owners=admitted.covered_owners;
  switch(admitted.failure) {
    case IntroLifecycleAdmissionFailure::none: report.failure=IntroLifecyclePreflightFailure::none; break;
    case IntroLifecycleAdmissionFailure::reader_coverage: report.failure=IntroLifecyclePreflightFailure::reader_coverage; break;
    case IntroLifecycleAdmissionFailure::component_coverage: report.failure=IntroLifecyclePreflightFailure::component_coverage; break;
    case IntroLifecycleAdmissionFailure::owner_coverage: report.failure=IntroLifecyclePreflightFailure::owner_coverage; break;
  }
  return report;
}

IntroDeferredReaderCoverageInventory IntroRuntime::reader_coverage_inventory() const {
  IntroDeferredReaderCoverageInventory result{.stage=reader_bracket_stage_,
                                              .entries={}};
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete)
    return result;
  const auto& directory=resources_.sources().directory();
  const auto family_for=[&](const IntroDeferredReaderWork& work) {
    if(work.source_directory_index==resources_.controller_index())
      return IntroDeferredReaderFamily::movie_controller;
    if(work.source_directory_index==resources_.member_index())
      return IntroDeferredReaderFamily::first_cut_sequence;
    if(work.source_directory_index==resources_.first_cut_index())
      return IntroDeferredReaderFamily::first_cut_list;
    if(work.source_directory_index==resources_.window_index())
      return IntroDeferredReaderFamily::window_owner;
    const auto legal=resources_.sources().local_source_for_authored_reference(
        resources_.member().references[1]);
    if(legal && *legal==work.source_directory_index)
      return IntroDeferredReaderFamily::first_cut_legal_picture;
    if(work.source_directory_index==466U)
      return IntroDeferredReaderFamily::external_cut_commands;
    if(work.source_directory_index<directory.size() &&
       directory[work.source_directory_index].source_type==0x00200012U)
      return IntroDeferredReaderFamily::sound_owner;
    for(const auto& command:resources_.first_cut().commands) {
      const auto target=resources_.sources().local_source_for_authored_reference(command.target_reference);
      if(target && *target==work.source_directory_index)
        return IntroDeferredReaderFamily::first_cut_fade_picture;
    }
    return IntroDeferredReaderFamily::unclassified;
  };
  for(const auto& work:deferred_reader_work_) {
    if(work.source_directory_index>=directory.size()) continue;
    const auto family=family_for(work);
    const IntroReaderAdmissionIdentity identity{work.resource.value,work.source_offset,
                                                 work.source_directory_index};
    const bool applied=std::ranges::find(supported_reader_admissions_,identity)!=
                       supported_reader_admissions_.end();
    const auto state=applied ? IntroDeferredReaderImplementationState::applied :
        family==IntroDeferredReaderFamily::unclassified ?
          IntroDeferredReaderImplementationState::unimplemented :
          IntroDeferredReaderImplementationState::implemented_not_applied;
    ++result.total_discovered;
    if(family!=IntroDeferredReaderFamily::unclassified) ++result.total_supported;
    if(applied) ++result.total_applied;
    const auto found=std::ranges::find_if(result.entries,[&](const auto& entry) {
      return entry.source_type==directory[work.source_directory_index].source_type &&
          entry.family==family && entry.state==state;
    });
    if(found==result.entries.end())
      result.entries.push_back({directory[work.source_directory_index].source_type,family,state,1U});
    else ++found->count;
  }
  std::ranges::sort(result.entries,{},[](const auto& entry) {
    return std::tuple{entry.source_type,entry.family,entry.state};
  });
  return result;
}

void IntroRuntime::record_supported_reader_admission(const IntroDeferredReaderWork& work) {
  if(work.source_directory_index>=directory_resource_mapping_.size() ||
      directory_resource_mapping_.at(work.source_directory_index)!=work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource)!=source_handle(work.source_directory_index))
    throw std::runtime_error("Supported reader admission requires a live matching owner");
  const IntroReaderAdmissionIdentity identity{work.resource.value,work.source_offset,work.source_directory_index};
  if(std::ranges::find(supported_reader_admissions_,identity)!=supported_reader_admissions_.end())
    throw std::runtime_error("Supported reader admission cannot be recorded twice");
  supported_reader_admissions_.push_back(identity);
}

void IntroRuntime::record_supported_owner_admission(std::size_t source, IntroRuntimeHandle owner) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      owner!=source_handle(source) || !sound_for_source(source).source_applied_)
    return;
  const auto resource=directory_resource_mapping_.at(source);
  if(!resource || !std::ranges::any_of(supported_reader_admissions_,[&](const auto& reader) {
       return reader.source_directory_index==source && reader.resource==resource->value;
     }))
    return;
  const IntroOwnerAdmissionIdentity identity{owner.value};
  if(std::ranges::find(supported_owner_admissions_,identity)!=supported_owner_admissions_.end())
    throw std::runtime_error("Supported owner admission cannot be recorded twice");
  supported_owner_admissions_.push_back(identity);
}

void IntroRuntime::record_supported_component_admission(std::size_t component_index) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete)
    throw std::runtime_error("Supported component admission requires complete directory construction");
  const auto& component=components_.at(component_index);
  if(!component.constructed() || component.removed() || component.source().synthesized ||
      !component.source().directory_index)
    throw std::runtime_error("Supported component admission requires a live authored component");
  const IntroComponentAdmissionIdentity identity{component_handle(component_index)};
  if(std::ranges::find(supported_component_admissions_,identity)!=supported_component_admissions_.end())
    throw std::runtime_error("Supported component admission cannot be recorded twice");
  supported_component_admissions_.push_back(identity);
}

FirstCutLegalPictureActivationResult IntroRuntime::activate_first_cut_legal_picture(
    const FirstCutLegalPictureActivationPrerequisites& prerequisites) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      !prerequisites.global_lifecycle_complete || !prerequisites.positive_time_member_activated ||
      !prerequisites.position_update_service)
    throw std::runtime_error("First-cut legal picture activation prerequisites are unavailable");
  const auto legal_source=resources_.sources().local_source_for_authored_reference(
      resources_.member().references[1]);
  if(!legal_source || *legal_source>=resources_.sources().directory().size())
    throw std::runtime_error("First-cut legal picture has no source-directory identity");
  auto& picture=picture_for_source(*legal_source);
  if(picture.handle()!=source_handle(*legal_source))
    throw std::runtime_error("First-cut legal picture has inconsistent live owner identity");
  const auto attachments=owner_components(picture.handle());
  std::optional<std::size_t> center;
  for(const auto component:attachments) {
    const auto& source=components_.at(component).source();
    if(source.factory_name!="ZGEOM_Center") continue;
    if(std::bit_cast<std::uint32_t>(source.authored_parameter)!=std::bit_cast<std::uint32_t>(1.0F) || center)
      throw std::runtime_error("First-cut legal picture Center shape is unsupported");
    center=component;
  }
  if(!center || !components_.at(*center).constructed() || components_.at(*center).removed() ||
      components_.at(*center).state().attached_owner!=picture.handle().value)
    throw std::runtime_error("First-cut legal picture Center is unavailable");
  bool requested{};
  cutscene::PictureActivationPrefix prefix;
  CenterPicturePosition center_position;
  prefix.run(prerequisites.picture_runtime_flags,prerequisites.parent_runtime_flags,
      prerequisites.owner_present,[&](cutscene::PictureActivationPrefix::Stage stage) {
        if(prerequisites.activation_stage) prerequisites.activation_stage(stage);
        if(stage==cutscene::PictureActivationPrefix::Stage::phase_one)
          center_position.initialize(prerequisites.picture_position,prerequisites.picture_runtime_flags,
              prerequisites.center_component_status,prerequisites.engine_width,prerequisites.engine_height,
              picture.submission_cache(),prerequisites.position_update_service);
        if(stage==cutscene::PictureActivationPrefix::Stage::record_requested) requested=true;
      });
  return {*legal_source,*center,requested};
}

void IntroRuntime::run_postconstruction_reader_bracket(
    std::uint64_t retained_saved_value,const IntroPostconstructionReaderServices& services) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      reader_bracket_stage_!=IntroReaderBracketStage::not_started)
    throw std::runtime_error("Post-construction reader bracket is unavailable at this loader stage");

  // The restore decision is a real branch. This approved boundary contains no
  // restore reader sequence, so it must not borrow the ordinary route.
  if(restore_mode_) {
    reader_bracket_retained_saved_value_=retained_saved_value;
    reader_bracket_stage_=IntroReaderBracketStage::restore_mode_selected;
    return;
  }

  const auto mapped_deferred=[this](const IntroDeferredReaderWork& work) {
    if(!work.resource.value || !work.source_offset || !associated_resource_owner(work.resource) ||
        !resource_state_for_handle(work.resource))
      return false;
    const auto& directory=resources_.sources().directory();
    return work.source_directory_index<directory.size() &&
        directory_resource_mapping_.at(work.source_directory_index)==work.resource &&
        directory[work.source_directory_index].deferred_source_offset==work.source_offset;
  };
  const auto mapped_script=[this](const IntroSourceScriptWork& work) {
    return work.resource.value && work.source_offset && associated_resource_owner(work.resource) &&
        resource_state_for_handle(work.resource).has_value();
  };
  const auto fail=[this] {
    reader_bracket_stage_=IntroReaderBracketStage::failed;
    resource_load_stage_=IntroResourceLoadStage::failed;
  };
  try {
    if(!services.external_loader_service || !services.pre_reader_service || !services.end_reader_service ||
        (!source_script_work_.empty() && !services.source_script_work) ||
        (!deferred_reader_work_.empty() && (!services.prepare_deferred_reader ||
            !services.owner_reader_boundary || !services.component_reader_boundary)))
      throw std::runtime_error("Post-construction reader bracket requires every available boundary service");

    reader_bracket_retained_saved_value_=retained_saved_value;
    services.external_loader_service(retained_saved_value);
    for(const auto& work:source_script_work_) {
      if(!mapped_script(work)) throw std::runtime_error("Queued source-script work lost its mapped owner");
      services.source_script_work(work);
    }
    services.external_loader_service(retained_saved_value);
    services.pre_reader_service();
    for(auto& work:deferred_reader_work_) {
      if(!mapped_deferred(work)) throw std::runtime_error("Deferred reader work lost its mapped owner or source span");
      prepare_deferred_references(work);
      services.prepare_deferred_reader(work);
      services.owner_reader_boundary(work);
      services.component_reader_boundary(work);
    }
    services.end_reader_service();
    reader_bracket_stage_=IntroReaderBracketStage::ordinary_reader_boundary_complete;
  } catch(...) {
    fail();
    throw;
  }
}

void IntroRuntime::run_outer_loader_tail_through_saved_services(
    const IntroOuterLoaderTailServices& services) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      reader_bracket_stage_!=IntroReaderBracketStage::ordinary_reader_boundary_complete ||
      outer_loader_tail_stage_!=IntroOuterLoaderTailStage::not_started)
    throw std::runtime_error("Outer loader tail is unavailable at this loader stage");

  // These are explicit external boundaries, not native stand-ins for their
  // still-unimplemented concrete readers and scene operations. Validate the
  // complete required ordinary path before doing any externally visible work.
  const bool named_requires_reader=services.named_global_payload.has_value();
  const bool renderer_requires_parser=services.renderer_resource_payload.has_value();
  const bool associations_require_services=!services.resource_associations.empty();
  if((named_requires_reader && (!services.relocate_named_global_references || !services.read_named_global_payload)) ||
      (renderer_requires_parser && (!services.parse_renderer_resource_payload ||
          !services.release_renderer_construction_reference)) ||
      (associations_require_services && !services.associate_live_resources) ||
      !services.release_loader_source_lease || !services.camera_zero_present ||
      !services.outer_scene_operation || !services.between_saved_scene_operation ||
      !services.intermediate_scene_finalization || !services.spatial_admission ||
      !services.saved_0x4000_service) {
    outer_loader_tail_stage_=IntroOuterLoaderTailStage::incomplete;
    throw std::runtime_error("Outer loader tail requires every concrete service boundary");
  }

  const auto resolve_saved=[this](const IntroSavedResourceFlags& saved) {
    if(!saved.resource.value || !associated_resource_owner(saved.resource) ||
        !resource_state_for_handle(saved.resource))
      throw std::runtime_error("Saved resource has no current live identity");
    return saved.resource;
  };
  try {
    if(const auto& named=services.named_global_payload) {
      const auto envelope=parse_intro_named_global_section_envelope(named->bytes);
      IntroNamedGlobalPreparedReader prepared{
          .owned_block={envelope.tagged_block.begin(),envelope.tagged_block.end()}};
      services.relocate_named_global_references(prepared);
      // The retail traversal restores its tagged-reader cursor before scene
      // dispatch. Preserve that observable handoff even when the native
      // relocation service used a cursor while walking its owned copy.
      prepared.reset_to_base();
      services.read_named_global_payload(envelope.label,prepared);
    }

    if(const auto& renderer=services.renderer_resource_payload) {
      const auto prepared=prepare_intro_renderer_relocation_payload(
          renderer->bytes,services.resolve_renderer_reference);
      const auto parsed=services.parse_renderer_resource_payload(prepared.bytes);
      if(!parsed.identity) throw std::runtime_error("Renderer-resource parser did not return a live container");
      // Retain the manager's logical ownership before releasing only the
      // construction reference. This is not renderer readiness.
      renderer_resource_container_=parsed;
      services.release_renderer_construction_reference(parsed);
    }

    constexpr std::uint32_t reference_domain_marker=0x40000000U;
    constexpr std::uint32_t association_reference_bias=0x70U;
    for(const auto& association:services.resource_associations) {
      // Both lookups are independent and deliberately happen even if the
      // first one misses; a miss merely suppresses the association callback.
      if(association.first_reference>
              std::numeric_limits<std::uint32_t>::max()-association_reference_bias ||
          association.second_reference>
              std::numeric_limits<std::uint32_t>::max()-association_reference_bias)
        throw std::runtime_error("Resource association reference overflows its runtime bias");
      const auto first=resolve_marked_source_resource_reference(
          (association.first_reference+association_reference_bias)|reference_domain_marker);
      const auto second=resolve_marked_source_resource_reference(
          (association.second_reference+association_reference_bias)|reference_domain_marker);
      if(first && second) services.associate_live_resources(*first,*second);
    }

    // Reset before each ordinary load. Absent source arrays stay absent rather
    // than gaining native placeholder records.
    first_auxiliary_array_.clear();
    second_auxiliary_array_.clear();
    if(services.auxiliary_arrays.first) first_auxiliary_array_=*services.auxiliary_arrays.first;
    if(services.auxiliary_arrays.second) second_auxiliary_array_=*services.auxiliary_arrays.second;

    // The public host retains prepared source views.  Releasing this logical
    // loader lease therefore cannot clear or invalidate those borrowed spans.
    services.release_loader_source_lease();
    loader_source_lease_released_=true;
    outer_loader_tail_stage_=IntroOuterLoaderTailStage::source_lease_released;

    if(!services.camera_zero_present()) {
      if(!services.enqueue_transform || !services.fallback_camera_registration.width ||
          !services.fallback_camera_registration.height ||
          !services.fallback_camera_registration.backend_ready) {
        outer_loader_tail_stage_=IntroOuterLoaderTailStage::incomplete;
        throw std::runtime_error("Default camera fallback requires concrete transform and registration services");
      }
      const auto fallback=ensure_default_camera(services.single_allocation_mode,
          services.enqueue_transform,services.fallback_camera_registration);
      if(!fallback || !registered_cameras_.camera_at(0,[this](std::uint64_t owner){return live_owner(owner);}))
        throw std::runtime_error("Default camera fallback did not register camera zero");
    }
    outer_loader_tail_stage_=IntroOuterLoaderTailStage::camera_zero_complete;

    // The three observed outer scene operations are opaque service boundaries.
    // They must precede spatial admission; no native scene state is fabricated.
    for(unsigned operation=0;operation<3;++operation) services.outer_scene_operation();
    for(const auto& saved:saved_resource_flags_)
      services.spatial_admission(resolve_saved(saved),true);
    outer_loader_tail_stage_=IntroOuterLoaderTailStage::first_saved_pass_complete;

    services.between_saved_scene_operation();
    services.intermediate_scene_finalization();
    for(const auto& saved:saved_resource_flags_)
      if(saved.flags&0x4000U) services.saved_0x4000_service(resolve_saved(saved),true);
    outer_loader_tail_stage_=IntroOuterLoaderTailStage::second_saved_pass_complete;
  } catch(...) {
    if(outer_loader_tail_stage_!=IntroOuterLoaderTailStage::incomplete) {
      outer_loader_tail_stage_=IntroOuterLoaderTailStage::failed;
      resource_load_stage_=IntroResourceLoadStage::failed;
    }
    throw;
  }
}

std::optional<IntroRuntimeResourceHandle>
IntroRuntime::resolve_marked_source_resource_reference(
    std::uint32_t reference) const {
  const auto source=resources_.sources().local_source_for_handle(reference);
  if(!source || *source>=directory_resource_mapping_.size()) return std::nullopt;
  const auto resource=directory_resource_mapping_[*source];
  if(!resource || !associated_resource_owner(*resource) ||
      !resource_state_for_handle(*resource)) return std::nullopt;
  return resource;
}

void IntroRuntime::construct_room_animation_scope_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::following_visual_scope_ready || loaded_resource_handles_.size()!=48 ||
      count_group_selector_!=5 || current_source_parent()!=source_handle(42) || resource_allocation_enabled_ ||
      components_.construction_mode() || manager_row_edit_ || scene_resource_edit_ || !directory_position_controls_prepared_ ||
      position_mode_.immediate || position_mode_.collection_enabled || position_updates_.failed())
    throw std::runtime_error("Room animation scope requires the completed visual scope and retained loader controls");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<69) throw std::runtime_error("Room animation sources are absent");
  constexpr std::array<std::uint32_t,21> types{
    0x100001,0x100021,0x100001,0x200002,0x200002,0x800024,0x800024,0x800024,0x800024,
    0x200002,0x200002,0x400003,0x400003,0x400003,0x200002,0x100001,0x2000e5,0x800001a,0x800001a,0x800001a,0x800001a};
  constexpr std::array<std::uint32_t,21> flags{
    0x03200000,0x07044000,0x03000000,0x20073,0x20071,0x80,0x80,0x80,0x80,
    0x200f3,0x200f1,0x280400,0x280400,0x280400,0x20073,0x47000000,0x200f3,0x200000,0x200000,0x200000,0x200000};
  constexpr std::array<std::uint32_t,21> pools{0,6,7,8,8,7,7,7,7,7,7,7,7,7,7,6,9,6,6,6,6};
  for(std::size_t row=48;row<=68;++row) {
    const auto& source=directory[row];
    const bool group=row==48 || row==49 || row==50 || row==63;
    const bool light=row>=53 && row<=56;
    const bool camera=row>=59 && row<=61;
    const bool no_reader=row==57 || row==58 || row==62;
    const bool metadata=(row>=51 && row<=58) || row==62 || row==64;
    const auto category=row==49?16U:group?0U:light?2U:camera || row>=65?3U:1U;
    const auto pop=row==48 || row==53 || row==63 || row==65?1U:0U;
    if(source.source_type!=types[row-48] || !application_.has_class_registration(source.source_type) ||
        source.object_flags!=flags[row-48] || source.pool_group!=pools[row-48] || source.pool_class!=category ||
        source.source_variant!=(row==49?2U:0U) || source.parent_steps!=pop || source.enters_child_pool!=group ||
        source.post_load_source_offset ||
        (source.child_value!=0)!=(row==49 || row==50 || row==63) ||
        (source.class_data_value!=0)!=metadata || (source.buf_auxiliary_offset!=0)!=camera ||
        (source.deferred_source_offset!=0)==no_reader ||
        !std::ranges::all_of(source.position,[](float value){return std::isfinite(value);}) ||
        !std::ranges::all_of(source.basis,[](float value){return std::isfinite(value);}))
      throw std::runtime_error("Unsupported Room animation source shape");
    const bool attached=row==51 || row==52 || camera || row>=65;
    if(source.attachments.size()!=(attached?1U:0U) || owner_components(source_handle(row)).size()!=source.attachments.size() ||
        (!attached && source.attachment_table_offset))
      throw std::runtime_error("Unsupported Room animation attachment count");
    if(attached) {
      const auto factory=row<=52?"ZSTDOBJ_VertAnim":camera?"ZGEOM_MatPosAnim":row==65?"ZLIST_CutSequenceList":"ZLIST_CutSequence";
      const auto raw=camera?0x42c80000U:row==65?0U:0x3f800000U;
      if(resources_.sources().attachment_identifier(row,0)!=factory || !application_.has_component_class_registration(factory) ||
          std::bit_cast<std::uint32_t>(source.attachments.front().parameter)!=raw)
        throw std::runtime_error("Unsupported Room animation attachment factory or raw argument");
    }
  }
  const auto& root_state=resource_state(root_handle());
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode || root_owner_state_->aggregate_flags ||
      !root_owner_state_->category_memberships.empty() || !root_owner_state_->rooms.empty() ||
      child_owners(root_handle())!=std::vector<IntroRuntimeHandle>{source_handle(0),source_handle(1),source_handle(6),source_handle(42)})
    throw std::runtime_error("Room animation scope requires retained fresh ROOT ancestry");
  try {
    for(std::size_t row=48;row<=68;++row) {
      const auto& source=directory[row];
      advance_source_loading_progress_without_engine_renderer(row);
      for(std::size_t pop=0;pop<source.parent_steps;++pop) {
        const auto parent=resource_parent(current_source_parent());
        if(!parent.value) throw std::runtime_error("Directory scope pop exceeds ROOT");
        current_source_parent_=resource_owner(parent);
      }
      if(std::ranges::find(source_resource_scopes_,source.pool_group,&IntroSourceResourceScope::count_group)==source_resource_scopes_.end()) {
        if(source.pool_group!=count_group_selector_) throw std::runtime_error("Directory scope allocation selector mismatch");
        allocate_source_scope(source.pool_group);
      }
      if(source.pool_class%8==0) construct_group_row_without_engine_renderer(row);
      else construct_non_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::room_animation_scope_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_following_visual_scope_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::second_window_scope_ready ||
      loaded_resource_handles_.size()!=42 || count_group_selector_!=4 ||
      resource_allocation_enabled_ || components_.construction_mode() || manager_row_edit_ || scene_resource_edit_ ||
      current_source_parent()!=source_handle(6))
    throw std::runtime_error("Following visual scope requires the completed second Window");
  if(!application_.has_class_registration(0x00100001U) || !application_.has_class_registration(0x0020003aU))
    throw std::runtime_error("Following visual scope requires Group and concrete visual registrations");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<48) throw std::runtime_error("Following visual scope sources are absent");
  const auto zero=[](float value){return std::bit_cast<std::uint32_t>(value)==0U;};
  for(std::size_t row=42;row<=47;++row) {
    const auto& source=directory[row];
    const bool group=row==42;
    const auto flags=group?0x03000000U:row<=44?0x00200000U:0U;
    if(source.source_type!=(group?0x00100001U:0x0020003aU) || source.source_variant ||
        source.parent_steps!=(group?1:0) || source.enters_child_pool!=group ||
        source.pool_group!=(group?0U:5U) || source.pool_class!=(group?0U:1U) || source.object_flags!=flags ||
        source.auxiliary_value || source.buf_auxiliary_offset || source.child_value || source.post_load_source_offset ||
        source.attachment_table_offset || !source.attachments.empty() || !owner_components(source_handle(row)).empty() ||
        !source.deferred_source_offset || !engine_identity_bits(source.basis) || !std::ranges::all_of(source.position,zero) ||
        ((group || row==44)?source.class_data_value!=0:source.class_data_value==0))
      throw std::runtime_error("Unsupported following visual scope source shape");
  }
  const auto& root_state=resource_state(root_handle());
  if(!root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
      root_owner_state_->aggregate_flags || !root_owner_state_->category_memberships.empty() ||
      hierarchy_[0].parent!=no_picture_transform_parent || !engine_identity_bits(hierarchy_[0].matrix) ||
      !std::ranges::all_of(hierarchy_[0].position,zero) ||
      resource_parent(source_handle(6))!=resource_handle(root_handle()) ||
      child_owners(root_handle())!=std::vector<IntroRuntimeHandle>{source_handle(0),source_handle(1),source_handle(6)})
    throw std::runtime_error("Following visual scope requires retained fresh ROOT ancestry");
  std::vector<IntroRuntimeHandle> children;
  for(std::size_t row=7;row<=41;++row) children.push_back(source_handle(row));
  if(child_owners(source_handle(6))!=children)
    throw std::runtime_error("Second Window child scope is not complete");
  try {
    advance_source_loading_progress_without_engine_renderer(42);
    current_source_parent_=root_handle();
    construct_group_row_without_engine_renderer(42);
    for(std::size_t row=43;row<=47;++row) {
      advance_source_loading_progress_without_engine_renderer(row);
      if(row==43) allocate_source_scope(count_group_selector_);
      construct_non_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::following_visual_scope_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}

void IntroRuntime::construct_second_window_scope_without_engine_renderer() {
  if(resource_load_stage_!=IntroResourceLoadStage::second_window_picture_ready ||
      loaded_resource_handles_.size()!=8 || count_group_selector_!=4 ||
      resource_allocation_enabled_ || components_.construction_mode() || manager_row_edit_ || scene_resource_edit_)
    throw std::runtime_error("Second Window scope requires its Window and first Picture");
  if(!directory_position_controls_prepared_ || position_mode_.immediate || position_mode_.collection_enabled || position_updates_.failed())
    throw std::runtime_error("Second Window scope requires established directory position controls");
  const auto& window=window_for_owner(current_source_parent());
  const auto& root_state=resource_state(root_handle());
  const auto& window_state=resource_state(window.group.owner);
  if(window.group.owner!=source_handle(6) || window.group.aggregate_flags ||
      !root_state || root_state->flags!=0x09000000U || root_state->context.value ||
      !root_owner_state_ || !root_owner_state_->enabled || root_owner_state_->room_mode ||
      root_owner_state_->aggregate_flags || !root_owner_state_->category_memberships.empty() ||
      !window_state || window_state->flags!=0x09000000U || window_state->context.value ||
      resource_parent(window.group.owner)!=resource_handle(root_handle()) ||
      child_owners(root_handle())!=std::vector<IntroRuntimeHandle>{source_handle(0),source_handle(1),source_handle(6)} ||
      child_owners(window.group.owner)!=std::vector<IntroRuntimeHandle>{source_handle(7)})
    throw std::runtime_error("Second Window child prefix is not retained");
  const auto& directory=resources_.sources().directory();
  if(directory.size()<42) throw std::runtime_error("Second Window scope sources are absent");
  constexpr std::array<std::size_t,13> characters{8,12,14,17,19,21,23,25,27,29,31,33,35};
  const auto exact=[](float actual,float expected){return std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(expected);};
  for(std::size_t row=8;row<=41;++row) {
    const auto& source=directory[row];
    const bool character=std::ranges::find(characters,row)!=characters.end();
    const bool picture=row==9 || row==36;
    const bool camera=row==10;
    const bool list=!character && !picture && !camera;
    const auto type=character?0x0020002dU:picture?0x00200046U:camera?0x00400003U:0x0800001aU;
    const bool hidden=row==8 || row==17 || row==18 || row==20 || row==21;
    const auto x=character?(row==8?80.0F:100.0F):0.0F;
    const auto y=character?(row==8?150.0F:300.0F):0.0F;
    if(source.source_type!=type || !application_.has_class_registration(type) || source.source_variant ||
        source.parent_steps || source.enters_child_pool || source.pool_group!=4 || source.pool_class!=(character || picture?1U:3U) ||
        (source.object_flags&0xfffffU)!=(hidden?0x400U:0U) || source.class_data_value || source.auxiliary_value ||
        source.buf_auxiliary_offset || source.child_value || source.post_load_source_offset || !source.deferred_source_offset ||
        !engine_identity_bits(source.basis) || !exact(source.position[0],x) || !exact(source.position[1],y) || !exact(source.position[2],0.0F))
      throw std::runtime_error("Unsupported second Window owner source");
    const auto expected_count=camera || row==8?0U:list?(row==11 || row==37 || row==38 || row==41?1U:2U):1U;
    if(source.attachments.size()!=expected_count || owner_components(source_handle(row)).size()!=expected_count ||
        (!expected_count && source.attachment_table_offset))
      throw std::runtime_error("Unsupported second Window attachment count");
    for(std::size_t slot=0;slot<source.attachments.size();++slot) {
      const auto expected=list?"ZLIST_ExternCutSequenceCommand":character?"ZCHAROBJ_CharFader":row==36?"ZWINPIC_LogoFade":"ZWINPIC_FadeToBlack";
      if(resources_.sources().attachment_identifier(row,slot)!=expected ||
          !application_.has_component_class_registration(expected) ||
          std::bit_cast<std::uint32_t>(source.attachments[slot].parameter)!=(list?0x3f800000U:0U))
        throw std::runtime_error("Unsupported second Window attachment factory or argument");
    }
  }
  try {
    for(std::size_t row=8;row<=41;++row) {
      advance_source_loading_progress_without_engine_renderer(row);
      construct_non_group_row_without_engine_renderer(row);
    }
    resource_load_stage_=IntroResourceLoadStage::second_window_scope_ready;
  } catch(...) {resource_load_stage_=IntroResourceLoadStage::failed;throw;}
}
IntroWindowOwner& IntroRuntime::window_for_owner(IntroRuntimeHandle owner) {
  static_cast<void>(hierarchy_index(owner));
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable Window");
  for(auto& [source,window]:window_owners_) {
    static_cast<void>(source);
    if(window->group.owner==owner) return *window;
  }
  throw std::runtime_error("Intro owner is not a constructed Window");
}
const IntroWindowOwner& IntroRuntime::window_for_owner(IntroRuntimeHandle owner) const {
  static_cast<void>(hierarchy_index(owner));
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable Window");
  for(const auto& [source,window]:window_owners_) {
    static_cast<void>(source);
    if(window->group.owner==owner) return *window;
  }
  throw std::runtime_error("Intro owner is not a constructed Window");
}

void IntroRuntime::apply_supported_window_deferred_reader(const IntroDeferredReaderWork& work) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete || projected_ ||
      !work.processed || work.source_directory_index>=directory_resource_mapping_.size())
    throw std::runtime_error("Supported Window reader is unavailable at this loader stage");

  const auto& sources=resources_.sources();
  const auto& directory=sources.directory();
  if(work.source_directory_index>=directory.size())
    throw std::runtime_error("Supported Window reader has no source-directory entry");
  const auto& source=directory[work.source_directory_index];
  if(source.source_type!=0x00100030U || source.class_data_value!=0U || !source.attachments.empty() ||
      !source.deferred_source_offset || source.deferred_source_offset!=work.source_offset ||
      directory_resource_mapping_[work.source_directory_index]!=work.resource)
    throw std::runtime_error("Deferred work is not the supported Window owner form");

  // Parse and resolve all identities before changing either owner.  This is a
  // deliberately narrow policy for the reviewed first-cut Window form.
  const auto decoded=sources.intro_window_source(work.source_directory_index);
  const auto selected_source=sources.local_source_for_authored_reference(decoded.selected_camera_reference);
  if(!selected_source || *selected_source!=resources_.camera_index() ||
      *selected_source>=directory_resource_mapping_.size() ||
      !directory_resource_mapping_[*selected_source])
    throw std::runtime_error("Supported Window reader has no mapped selected camera");
  const auto resolve_optional_reference=[&](std::uint32_t reference)
      ->std::optional<IntroRuntimeResourceHandle> {
    if(reference==0U) return std::nullopt;
    const auto target=sources.local_source_for_authored_reference(reference);
    if(!target || *target>=directory_resource_mapping_.size() || !directory_resource_mapping_[*target])
      throw std::runtime_error("Supported Window reader has an unresolved opaque source reference");
    return *directory_resource_mapping_[*target];
  };
  // Resolve the complete pair before changing any live Window or camera state.
  std::array<std::optional<IntroRuntimeResourceHandle>,2> opaque_reference_resources;
  for(std::size_t index=0;index<decoded.opaque_references.size();++index)
    opaque_reference_resources[index]=resolve_optional_reference(decoded.opaque_references[index]);
  const auto window_handle=source_handle(work.source_directory_index);
  const auto camera_handle=source_handle(*selected_source);
  auto& window=window_for_owner(window_handle);
  auto& selected_camera=camera_for_owner(camera_handle);
  if(window.group.resource!=work.resource || resource_handle(camera_handle)!=*directory_resource_mapping_[*selected_source])
    throw std::runtime_error("Supported Window reader mapped an inconsistent live owner");
  if(!selected_camera.can_apply_window_state_projection(window_handle.value))
    throw std::runtime_error("Supported Window reader requires a stable selected camera");
  const auto& basis=hierarchy_.at(hierarchy_index(window_handle)).matrix;
  if(!engine_identity_bits(basis))
    throw std::runtime_error("Supported Window reader requires unchanged engine identity orientation");

  // The owner-local collection is ordered and never duplicates the selected
  // camera. The following mutations preserve the reviewed reader sequence.
  if(std::ranges::find(window.cameras,camera_handle)==window.cameras.end())
    window.cameras.push_back(camera_handle);
  window.opaque_reference_resources=opaque_reference_resources;
  window.group.flags|=0x400U;
  selected_camera.begin_window_state_projection(decoded.options[0]!=0U,decoded.options[1]!=0U);
  window.pending_visibility=-1.0F;
  selected_camera.complete_window_state_projection(window_handle.value);
  window.group.flags|=0x400U;
  projected_=true;
  record_supported_reader_admission(work);
}

FreshIntroCamera& IntroRuntime::camera() {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable camera");
  if(const auto found=live_cameras_.find(resources_.camera_index());found!=live_cameras_.end()) return found->second->camera;
  if(resource_load_stage_==IntroResourceLoadStage::prepared) return prepared_camera_;
  throw std::runtime_error("Authored camera has not been constructed");
}
const FreshIntroCamera& IntroRuntime::camera() const {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable camera");
  if(const auto found=live_cameras_.find(resources_.camera_index());found!=live_cameras_.end()) return found->second->camera;
  if(resource_load_stage_==IntroResourceLoadStage::prepared) return prepared_camera_;
  throw std::runtime_error("Authored camera has not been constructed");
}

void IntroRuntime::stop_sound_owner(std::size_t source) {
  if (sound_preparation_busy_) throw std::runtime_error("Reentrant intro sound control is unsupported");
  auto& owner=sound_for_source(source);
  static_cast<void>(owner.record());
  if (owner.active_) {
    application_.sound_records().stop(owner.owner_binding_);
    owner.active_=false;
  }
}

void IntroRuntime::prepare_sound_owner(std::size_t source,
    const IntroSoundPreparationServices& services) {
  if (sound_preparation_busy_) throw std::runtime_error("Reentrant intro sound preparation is unsupported");
  if (!services.resource_flags || !services.parent_owner || !services.spatial_state ||
      !services.owner_enable_requested || !services.enable_owner)
    throw std::runtime_error("Missing live intro sound-owner preparation service");
  if (!application_.clock().ready() || application_.clock().failed())
    throw std::runtime_error("Intro sound preparation requires a valid application clock");
  auto& owner = sound_for_source(source);
  auto& record = owner.record();
  const auto& bank = resources_.sound_bank();
  if (!bank || !record.active_source)
    throw std::runtime_error("Intro sound preparation requires an assigned SND source");
  struct BusyGuard {
    bool& flag;
    explicit BusyGuard(bool& value) : flag(value) { flag=true; }
    ~BusyGuard() { flag=false; }
  } guard(sound_preparation_busy_);
  try {
    record.start_time = std::bit_cast<std::uint32_t>(application_.clock().state().raw_integer);
    record.parent = services.parent_owner(owner.handle_).value;
    owner.owner_binding_ = owner.lease_.binding();
    record.alternate_source = *record.active_source;
    if ((services.resource_flags(owner.handle_) & 0x400U) != 0) return;
    record.playback_state = 3;
    if (!application_.sound_records().prepare(owner.owner_binding_,*bank,
          std::bit_cast<std::uint32_t>(application_.clock().state().raw_integer))) {
      record.flags &= 0x2U;
      if (record.flags != 0)
        throw std::runtime_error("Failed intro sound preparation requires unsupported owner disposal");
      return;
    }
    const auto spatial = services.spatial_state(owner.handle_);
    for (float value : spatial.position)
      if (!std::isfinite(value)) throw std::runtime_error("Invalid live sound position");
    for (float value : spatial.direction)
      if (!std::isfinite(value)) throw std::runtime_error("Invalid live sound direction");
    record.position = spatial.position;
    record.direction = spatial.direction;
    if (services.owner_enable_requested()) services.enable_owner(owner.handle_);
    owner.active_ = true;
    record_supported_owner_admission(source,owner.handle_);
  } catch (...) {
    // Preserve completed mutations for diagnosis, but prohibit further owner
    // callbacks. Whole-host destruction releases leases after component captures.
    owner.failed_ = true;
    throw;
  }
}

void IntroRuntime::apply_sound_extension(std::size_t source) {
  if (sound_preparation_busy_)
    throw std::runtime_error("sound extension cannot reenter owner preparation");
  auto& owner=sound_for_source(source);
  if (owner.failed_) throw std::runtime_error("sound owner is unavailable");
  if (owner.owner_binding_==0) return;
  const auto& parameters=owner.source_->attachments.extend;
  if (parameters.scalars!=std::array<float,6>{-1,0,0,0,0,0} ||
      parameters.integers!=std::array<std::uint32_t,4>{0,0,0,0} ||
      parameters.option || parameters.category!=2 || parameters.option_a!=1 ||
      parameters.option_b!=1 || parameters.authored_output_mode!=0)
    throw std::runtime_error("intro sound extension requires unsupported parameter branches");
  auto& record=owner.record();
  if (record.binding!=owner.owner_binding_)
    throw std::runtime_error("sound extension binding is not the canonical owner record");
  // Keep the binary32 division before the double-precision power operation.
  volatile float exponent=parameters.scalars[0]/-20.0F;
  record.gain_multiplier=static_cast<float>(100.0/std::pow(10.0,static_cast<double>(exponent)));
  record.flags|=0x80U;
  record.category=parameters.category;
  // Authored options 1 preserve the current option bits; they do not force on.
  record.output_mode=2;
}

const IntroSoundFamilyPhaseOneResult& IntroRuntime::run_isolated_sound_family_phase_one() {
  if(isolated_sound_family_phase_one_busy_ || isolated_sound_family_phase_one_failed_ ||
      isolated_sound_family_phase_one_)
    throw std::runtime_error("Isolated intro sound phase one cannot run twice");
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      components_.failed() || components_.phases_completed() || sounds_.size()!=2U)
    throw std::runtime_error("Isolated intro sound phase one is unavailable for this scene");
  struct BusyGuard {
    bool& value;
    explicit BusyGuard(bool& supplied) : value(supplied) { value=true; }
    ~BusyGuard() { value=false; }
  } guard(isolated_sound_family_phase_one_busy_);
  try {
    IntroSoundFamilyPhaseOneResult result;
    constexpr std::array<std::string_view,4> factories{
        "ZSNDOBJ_SoundExtend","ZSNDOBJ_SoundNotify","ZSNDOBJ_SoundSegment","ZGEOM_ZSetZDefine"};
    std::array<std::size_t,8> callback_order{};
    for(std::size_t reverse=0;reverse<sounds_.size();++reverse) {
      auto& sound=*sounds_.at(sounds_.size()-1U-reverse);
      const auto source=sound.source_index();
      if(!sound.source_applied_ || !sound.has_record() || !sound.owner_binding_ || sound.failed_ || !sound.active_)
        throw std::runtime_error("Isolated intro sound phase one requires prepared reader-backed owners");
      const auto attachment_indices=owner_components(sound.handle());
      if(attachment_indices.size()!=factories.size())
        throw std::runtime_error("Isolated intro sound phase one attachment count is unsupported");
      for(std::size_t index=0;index<attachment_indices.size();++index) {
        const auto& component=components_.at(attachment_indices[index]);
        if(!component.constructed() || component.removed() || component.source().factory_name!=factories[index] ||
            component.state().attached_owner!=sound.handle().value)
          throw std::runtime_error("Isolated intro sound phase one attachment is unavailable");
      }
      auto& extend=constructed_picture_components_.at(attachment_indices[0]).sound_extend;
      auto& notify=constructed_picture_components_.at(attachment_indices[1]).sound_notify;
      auto& segment=constructed_picture_components_.at(attachment_indices[2]).sound_segment;
      auto& define=constructed_picture_components_.at(attachment_indices[3]).sound_define;
      if(!extend || !notify || !segment || !define || define->property_on_parent || define->property_key.empty() ||
          !std::isfinite(sound.record().duration))
        throw std::runtime_error("Isolated intro sound phase one reader state is unsupported");
      auto& output=result.owners[reverse];
      output={source,attachment_indices[0],attachment_indices[1],attachment_indices[2],attachment_indices[3],
              sound.record().duration,restore_mode_,false,define->property_key};
      // Real first-phase order: define, segment, notify, extend.  The scoped
      // runtime preflights all eight typed callbacks before the first one
      // runs; it provides no owner lookup, retirement or global completion.
      const auto start=reverse*4U;
      callback_order[start]=attachment_indices[3];
      callback_order[start+1U]=attachment_indices[2];
      callback_order[start+2U]=attachment_indices[1];
      callback_order[start+3U]=attachment_indices[0];
    }
    components_.run_scoped_phase_one(callback_order);
    for(std::size_t reverse=0;reverse<sounds_.size();++reverse) {
      auto& output=result.owners[reverse];
      const auto payload=constructed_picture_components_.find(output.extend_component);
      if(payload==constructed_picture_components_.end() || !payload->second.sound_extend)
        throw std::runtime_error("SoundExtend phase one lost its typed payload");
      output.extend_ordinary_removed=payload->second.sound_extend->phase_one_ordinary_removed;
    }
    isolated_sound_family_phase_one_=std::move(result);
    return *isolated_sound_family_phase_one_;
  } catch(...) {
    isolated_sound_family_phase_one_failed_=true;
    throw;
  }
}

std::span<const std::size_t> IntroRuntime::owner_components(IntroRuntimeHandle owner) const {
  return owner_components_.at(hierarchy_index(owner));
}
std::uint64_t IntroRuntime::component_handle(std::size_t index) const {
  const auto serial=components_.at(index).identity();
  if(!serial) throw std::runtime_error("Component has not run its common constructor");
  return std::uint64_t{*serial}+1;
}
void IntroRuntime::register_ordinary_component(std::size_t index) {
  const auto handle=component_handle(index);
  if(!(components_.at(index).state().admitted&0x10U))
    throw std::runtime_error("Ordinary registration requires actual component admission");
  if(!ordinary_) ordinary_=std::make_unique<runtime::OrdinaryComponentManager>(application_.ordinary_sorting(),
    runtime::OrdinaryMembershipServices{
      [this](std::uint64_t value)->runtime::ComponentRecord* {
        for(const auto component:components_.construction_order()) {
          auto& record=components_.at(component);
          if(record.identity() && std::uint64_t{*record.identity()}+1==value) return &record;
        }
        return nullptr;
      },
      [this](std::uint64_t value)->std::optional<runtime::OrdinaryOwner> {
        if(!live_owner(value)) return std::nullopt;
        return runtime::OrdinaryOwner{value,std::nullopt};
      },{}});
  ordinary_->enqueue(handle);
}

void IntroRuntime::register_camera(float key,const IntroCameraRegistrationServices& services) {
  register_camera(source_handle(resources_.camera_index()),key,services);
}
void IntroRuntime::register_camera(IntroRuntimeHandle owner,float key,const IntroCameraRegistrationServices& services) {
  if(!live_owner(owner.value)) throw std::runtime_error("Camera owner has not been constructed and associated");
  if(default_camera_==owner && (default_camera_busy_ || default_camera_failed_))
    throw std::runtime_error("Default camera construction has not completed successfully");
  auto* camera=&camera_for_owner(owner);
  registered_cameras_.register_camera(owner.value,key,{
    [this,owner](std::uint64_t handle) {
      return handle==owner.value && live_owner(handle);
    },
    [camera,services](std::uint64_t) {camera->notify_renderer_dimensions(services.width,services.height);},
    services.backend_ready,
    services.admit_view?std::function<void(std::uint64_t)>{
      [services](std::uint64_t handle) {services.admit_view({handle});}}:std::function<void(std::uint64_t)>{}
  });
}
FreshIntroCamera& IntroRuntime::camera_for_owner(IntroRuntimeHandle owner) {
  static_cast<void>(hierarchy_index(owner));
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable camera");
  for(auto& [source,live]:live_cameras_) {
    static_cast<void>(source);
    if(live->metadata.owner==owner) return live->camera;
  }
  if(owner==source_handle(resources_.camera_index())) return camera();
  if(default_camera_==owner) return *default_camera_owner_;
  throw std::runtime_error("Intro owner is not a constructed camera");
}
const FreshIntroCamera& IntroRuntime::camera_for_owner(IntroRuntimeHandle owner) const {
  static_cast<void>(hierarchy_index(owner));
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Failed intro construction has no usable camera");
  for(const auto& [source,live]:live_cameras_) {
    static_cast<void>(source);
    if(live->metadata.owner==owner) return live->camera;
  }
  if(owner==source_handle(resources_.camera_index())) return camera();
  if(default_camera_==owner) return *default_camera_owner_;
  throw std::runtime_error("Intro owner is not a constructed camera");
}
void IntroRuntime::set_camera_context(IntroRuntimeHandle context) {
  set_camera_context(source_handle(resources_.camera_index()),context);
}
void IntroRuntime::set_camera_context(IntroRuntimeHandle owner,IntroRuntimeHandle context) {
  static_cast<void>(camera_for_owner(owner));
  if(context.value) static_cast<void>(hierarchy_index(context));
  if(default_camera_==owner) default_camera_context_=context;
  else {
    for(auto& [source,live]:live_cameras_) {
      static_cast<void>(source);
      if(live->metadata.owner==owner) {live->context=context;return;}
    }
    prepared_camera_context_=context;
  }
}
IntroRuntimeHandle IntroRuntime::camera_context() const {
  if(resource_load_stage_==IntroResourceLoadStage::prepared) return prepared_camera_context_;
  // Before this factory, context remains a constructor input rather than proof
  // of a live camera. Preserve the first-cut convenience query's old contract.
  const auto found=live_cameras_.find(resources_.camera_index());
  return found==live_cameras_.end()?root_handle():found->second->context;
}
IntroRuntimeHandle IntroRuntime::camera_context(IntroRuntimeHandle owner) const {
  static_cast<void>(camera_for_owner(owner));
  if(default_camera_==owner) return default_camera_context_;
  for(const auto& [source,live]:live_cameras_) {
    static_cast<void>(source);
    if(live->metadata.owner==owner) return live->context;
  }
  return prepared_camera_context_;
}
void IntroRuntime::set_sound_listener(IntroRuntimeHandle owner) {
  application_.sound_records().set_listener(owner.value,[this](std::uint64_t handle) {
    return live_owner(handle);
  });
}
std::optional<IntroSoundListener> IntroRuntime::sound_listener() {
  const auto live=[this](std::uint64_t handle) {return live_owner(handle);};
  const auto handle=application_.sound_records().resolve_listener(live,[&] {
    return registered_cameras_.camera_at(0,live);
  });
  if(!handle) return std::nullopt;
  const auto selected_context=camera_context({handle});
  const auto context=selected_context.value?selected_context:root_handle();
  static_cast<void>(hierarchy_index(context));
  return IntroSoundListener{{handle},context};
}

void IntroRuntime::run_controller_phase_two(const IntroControllerPhaseTwoServices& external) {
  auto bound=external;
  application_.bind_controller_phase_two(bound);
  controller_initialization_.run_phase_two(bound);
}

runtime::ComponentCallback IntroRuntime::controller_phase_two_callback(
    const IntroControllerPhaseTwoServices& external) {
  return [this, external](runtime::ComponentRecord& record) {
    if (&record != &components_.at(controller_component_))
      throw std::runtime_error("MovieControl callback bound to a different component");
    run_controller_phase_two(external);
  };
}

void IntroRuntime::apply_supported_movie_control_deferred_reader(const IntroDeferredReaderWork& work) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      !work.processed || work.source_directory_index!=resources_.controller_index() ||
      work.source_offset!=resources_.sources().directory().at(work.source_directory_index).deferred_source_offset ||
      work.resource!=directory_resource_mapping_.at(work.source_directory_index).value_or(IntroRuntimeResourceHandle{}))
    throw std::runtime_error("MovieControl reader requires its live deferred owner work");
  auto& component=components_.at(controller_component_);
  if(!component.constructed() || component.source().factory_name!="ZGEOM_MovieControl")
    throw std::runtime_error("MovieControl reader has no constructed controller component");
  if(movie_controller_reader_state_)
    throw std::runtime_error("MovieControl source reader cannot run twice");
  const auto resolve=[this](std::uint32_t reference) -> IntroRuntimeResourceHandle {
    const auto source=resources_.sources().local_source_for_authored_reference(reference);
    if(!source || *source>=directory_resource_mapping_.size() || !directory_resource_mapping_[*source])
      throw std::runtime_error("MovieControl reader has an unresolved mandatory source reference");
    return *directory_resource_mapping_[*source];
  };
  const auto resolve_optional=[&resolve](std::uint32_t reference)
      -> std::optional<IntroRuntimeResourceHandle> {
    if(reference==0U) return std::nullopt;
    return resolve(reference);
  };
  const auto translate=[&resolve](std::span<const std::uint32_t> references) {
    std::vector<std::optional<IntroRuntimeResourceHandle>> result;
    result.reserve(references.size());
    for(const auto reference:references) {
      if(reference==0U) result.push_back(std::nullopt);
      else result.push_back(resolve(reference));
    }
    return result;
  };
  const auto& authored=resources_.controller();
  IntroMovieControllerReaderState state{
      .owner=source_handle(work.source_directory_index), .resource=work.resource,
      .component_index=controller_component_, .authored=authored,
      .sequence_list_resource=resolve(authored.sequence_reference),
      .group_list_resource=resolve(authored.group_reference),
      .additional_resource=resolve_optional(authored.additional_reference),
      .first_optional_resource=authored.first_optional_reference
          ?resolve_optional(*authored.first_optional_reference):std::nullopt,
      .second_optional_resource=authored.second_optional_reference
          ?resolve_optional(*authored.second_optional_reference):std::nullopt,
      .sequence_members=translate(resources_.cut_references()),
      .group_members=translate(resources_.group_references())};
  movie_controller_reader_state_=std::move(state);
  try { record_supported_reader_admission(work); }
  catch(...) { movie_controller_reader_state_.reset(); throw; }
}

void IntroRuntime::apply_supported_movie_control_component_reader(
    const IntroDeferredReaderWork& work) {
  if (resource_load_stage_ != IntroResourceLoadStage::directory_construction_complete ||
      !work.processed || work.source_directory_index != resources_.controller_index() ||
      !movie_controller_reader_state_ || movie_controller_component_reader_state_)
    throw std::runtime_error("MovieControl component reader requires its completed owner reader");
  const auto& component = components_.at(controller_component_);
  const auto* constructed = constructed_attachment(controller_component_);
  if (!component.constructed() || component.removed() ||
      component.source().factory_name != "ZGEOM_MovieControl" ||
      component.state().attached_owner != movie_controller_reader_state_->owner.value ||
      !constructed || !constructed->movie_control)
    throw std::runtime_error("MovieControl component reader has no live constructed controller");
  movie_controller_component_reader_state_ = {
      .owner = movie_controller_reader_state_->owner,
      .component_index = controller_component_,
      .class_ordinal = component.state().class_ordinal,
      .requested_mask = component.state().requested,
      .priority = component.state().priority,
      .events = constructed->movie_control->events,
  };
}

void IntroRuntime::apply_supported_first_cut_sequence_deferred_reader(
    const IntroDeferredReaderWork& work) {
  const auto source_index=resources_.member_index();
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete || !work.processed ||
      work.source_directory_index!=source_index || first_cut_sequence_reader_state_)
    throw std::runtime_error("First-cut sequence reader requires its unique live deferred owner work");
  if(std::ranges::find_if(deferred_reader_work_,[&](const auto& candidate) {
       return std::addressof(candidate)==std::addressof(work);
     })==deferred_reader_work_.end())
    throw std::runtime_error("First-cut sequence reader requires bracket-owned work");
  const auto& source=resources_.sources().directory().at(source_index);
  if(source.source_type!=0x0800001aU || source.deferred_source_offset!=work.source_offset ||
      directory_resource_mapping_.at(source_index)!=work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource)!=source_handle(source_index) ||
      source.attachments.size()!=1U || source.attachments[0].parameter!=1.0F ||
      resources_.sources().attachment_identifier(source_index,0)!="ZLIST_CutSequence")
    throw std::runtime_error("First-cut sequence reader source shape is unsupported");
  const auto attachments=owner_components(source_handle(source_index));
  if(attachments.size()!=1U)
    throw std::runtime_error("First-cut sequence reader attachment count is unsupported");
  const auto component_index=attachments.front();
  const auto& component=components_.at(component_index);
  if(!component.constructed() || component.removed() ||
      component.source().factory_name!="ZLIST_CutSequence" ||
      component.state().attached_owner!=source_handle(source_index).value)
    throw std::runtime_error("First-cut sequence reader component is unavailable");
  const auto resolve_optional=[this](std::uint32_t reference) -> std::optional<IntroRuntimeResourceHandle> {
    if(reference==0U) return std::nullopt;
    const auto source=resources_.sources().local_source_for_authored_reference(reference);
    if(!source || *source>=directory_resource_mapping_.size() || !directory_resource_mapping_[*source])
      throw std::runtime_error("First-cut sequence reader has an unresolved source reference");
    return *directory_resource_mapping_[*source];
  };
  const auto& authored=resources_.member();
  IntroFirstCutSequenceReaderState state{
      .owner=source_handle(source_index), .resource=work.resource,
      .component_index=component_index, .authored=authored, .members={}};
  for(std::size_t index=0;index<authored.references.size();++index)
    state.members[index]=resolve_optional(authored.references[index]);
  first_cut_sequence_reader_state_=std::move(state);
  try { record_supported_reader_admission(work); }
  catch(...) { first_cut_sequence_reader_state_.reset(); throw; }
}

void IntroRuntime::apply_supported_first_cut_list_deferred_reader(
    const IntroDeferredReaderWork& work) {
  const auto source_index=resources_.first_cut_index();
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete || !work.processed ||
      work.source_directory_index!=source_index || first_cut_list_reader_state_)
    throw std::runtime_error("First-cut list reader requires its unique live deferred owner work");
  if(std::ranges::find_if(deferred_reader_work_,[&](const auto& candidate) {
       return std::addressof(candidate)==std::addressof(work);
     })==deferred_reader_work_.end())
    throw std::runtime_error("First-cut list reader requires bracket-owned work");
  const auto& source=resources_.sources().directory().at(source_index);
  constexpr std::array<std::string_view,6> factories{
      "ZLIST_CutSequenceList", "ZLIST_CutSequenceCommand", "ZLIST_CutSequenceCommand",
      "ZLIST_CutSequenceCommand", "ZLIST_CutSequenceCommand", "ZLIST_CutSequenceCommand"};
  if(source.source_type!=0x0800001aU || source.deferred_source_offset!=work.source_offset ||
      directory_resource_mapping_.at(source_index)!=work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource)!=source_handle(source_index) ||
      source.attachments.size()!=factories.size())
    throw std::runtime_error("First-cut list reader source shape is unsupported");
  const auto attachments=owner_components(source_handle(source_index));
  if(attachments.size()!=factories.size())
    throw std::runtime_error("First-cut list reader attachment count is unsupported");
  for(std::size_t index=0;index<factories.size();++index) {
    const auto expected_parameter=index==0?0.0F:1.0F;
    const auto& component=components_.at(attachments[index]);
    if(source.attachments[index].parameter!=expected_parameter ||
        resources_.sources().attachment_identifier(source_index,index)!=factories[index] ||
        !component.constructed() || component.removed() ||
        component.source().factory_name!=factories[index] ||
        component.state().attached_owner!=source_handle(source_index).value)
      throw std::runtime_error("First-cut list reader attachment is unsupported");
  }
  const auto resolve_required=[this](std::uint32_t reference) -> IntroRuntimeResourceHandle {
    const auto source=resources_.sources().local_source_for_authored_reference(reference);
    if(!source || *source>=directory_resource_mapping_.size() || !directory_resource_mapping_[*source])
      throw std::runtime_error("First-cut list reader has an unresolved mandatory source reference");
    return *directory_resource_mapping_[*source];
  };
  const auto& authored=resources_.first_cut();
  IntroFirstCutListReaderState state{
      .owner=source_handle(source_index), .resource=work.resource,
      .component_indices={attachments[0],attachments[1],attachments[2],attachments[3],attachments[4],attachments[5]},
      .authored=authored, .sequence_resource=resolve_required(authored.sequence_reference), .command_target_resources={}};
  for(std::size_t index=0;index<authored.commands.size();++index)
    state.command_target_resources[index]=resolve_required(authored.commands[index].target_reference);
  first_cut_list_reader_state_=std::move(state);
  try { record_supported_reader_admission(work); }
  catch(...) { first_cut_list_reader_state_.reset(); throw; }
}

void IntroRuntime::prepare_supported_first_cut_player() {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      !first_cut_sequence_reader_state_ || !first_cut_list_reader_state_ ||
      first_cut_player_prepared_state_)
    throw std::runtime_error("First-cut player requires both unconsumed reader states");
  const auto& sequence=*first_cut_sequence_reader_state_;
  const auto& list=*first_cut_list_reader_state_;
  if(list.sequence_resource!=sequence.resource ||
      list.component_indices[0]>=components_.size() ||
      sequence.component_index>=components_.size())
    throw std::runtime_error("First-cut player reader states do not share a live sequence");
  const auto& list_component=components_.at(list.component_indices[0]);
  const auto& sequence_component=components_.at(sequence.component_index);
  if(!list_component.constructed() || list_component.removed() ||
      list_component.source().factory_name!="ZLIST_CutSequenceList" ||
      list_component.state().attached_owner!=list.owner.value ||
      !sequence_component.constructed() || sequence_component.removed() ||
      sequence_component.source().factory_name!="ZLIST_CutSequence" ||
      sequence_component.state().attached_owner!=sequence.owner.value)
    throw std::runtime_error("First-cut player has no live list or sequence component");
  const auto& settings=list.authored.settings_words;
  if(!std::isfinite(list.authored.final_value) ||
      !std::isfinite(sequence.authored.values[0]) || !std::isfinite(sequence.authored.values[1]))
    throw std::runtime_error("First-cut player source float is not finite");
  IntroFirstCutPlayerPreparedState state{
      .list_owner=list.owner, .sequence_owner=sequence.owner,
      .list_resource=list.resource, .sequence_resource=sequence.resource,
      .list_component_index=list.component_indices[0], .sequence_component_index=sequence.component_index,
      .leading_controls={settings[0],settings[1],settings[2]},
      .raw_scalar=settings[3],
      .trailing_controls={settings[4],settings[5],settings[6]},
      .list_value=list.authored.final_value, .sequence_values=sequence.authored.values,
      .raw_enabled_option=sequence.authored.authored_option,
      .started={},.completed={}};
  first_cut_player_prepared_state_=std::move(state);
}

cutscene::FirstCutPlayerInitialization IntroRuntime::first_cut_player_initialization() const {
  if (!first_cut_player_prepared_state_ || !first_cut_sequence_reader_state_ ||
      !first_cut_list_reader_state_)
    throw std::runtime_error("First-cut player initialization requires prepared reader state");
  const auto& player = *first_cut_player_prepared_state_;
  const auto& list = *first_cut_list_reader_state_;
  const auto& sequence = *first_cut_sequence_reader_state_;
  if (player.list_component_index != list.component_indices[0] ||
      player.sequence_component_index != sequence.component_index ||
      player.list_resource != list.resource || player.sequence_resource != sequence.resource)
    throw std::runtime_error("First-cut player prepared state no longer matches its readers");
  return cutscene::FirstCutPlayerInitialization({
      .list_component = player.list_component_index,
      .command_components = {list.component_indices[1], list.component_indices[2],
                             list.component_indices[3], list.component_indices[4],
                             list.component_indices[5]},
      .list = list.authored,
      .sequence = sequence.authored});
}

cutscene::FirstCutPlayerSession IntroRuntime::first_cut_player_session() const {
  if (!first_cut_player_prepared_state_ || !first_cut_sequence_reader_state_ ||
      !first_cut_list_reader_state_)
    throw std::runtime_error("First-cut player session requires prepared reader state");
  const auto& player = *first_cut_player_prepared_state_;
  const auto& list = *first_cut_list_reader_state_;
  const auto& sequence = *first_cut_sequence_reader_state_;
  if (player.list_component_index != list.component_indices[0] ||
      player.sequence_component_index != sequence.component_index ||
      player.list_resource != list.resource || player.sequence_resource != sequence.resource)
    throw std::runtime_error("First-cut player prepared state no longer matches its readers");
  return cutscene::FirstCutPlayerSession({
      .list_component = player.list_component_index,
      .command_components = {list.component_indices[1], list.component_indices[2],
                             list.component_indices[3], list.component_indices[4],
                             list.component_indices[5]},
      .list = list.authored,
      .sequence = sequence.authored});
}

void IntroRuntime::apply_supported_external_cut_commands_deferred_reader(
    const IntroDeferredReaderWork& work) {
  constexpr std::array<std::string_view,2> factories{
      "ZLIST_ExternCutSequenceCommand", "ZLIST_ExternCutSequenceCommand"};
  constexpr std::size_t reviewed_source_index = 466U;
  if (resource_load_stage_ != IntroResourceLoadStage::directory_construction_complete ||
      !work.processed || external_cut_commands_reader_state_ ||
      work.source_directory_index != reviewed_source_index ||
      reviewed_source_index >= resources_.sources().directory().size() ||
      std::ranges::find_if(deferred_reader_work_, [&](const auto &candidate) {
        return std::addressof(candidate) == std::addressof(work);
      }) == deferred_reader_work_.end())
    throw std::runtime_error("External cut command reader requires its unique live deferred owner work");
  const auto &source = resources_.sources().directory().at(reviewed_source_index);
  if (source.source_type != 0x0800001aU ||
      source.deferred_source_offset != work.source_offset ||
      directory_resource_mapping_.at(reviewed_source_index) != work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource) != source_handle(reviewed_source_index) ||
      source.attachments.size() != factories.size())
    throw std::runtime_error("External cut command reader source shape is unsupported");
  const auto attachments = owner_components(source_handle(reviewed_source_index));
  if (attachments.size() != factories.size())
    throw std::runtime_error("External cut command reader attachment count is unsupported");
  for (std::size_t index = 0; index < factories.size(); ++index) {
    const auto &component = components_.at(attachments[index]);
    if (source.attachments[index].parameter != 1.0F ||
        resources_.sources().attachment_identifier(reviewed_source_index, index) != factories[index] ||
        !component.constructed() || component.removed() ||
        component.source().factory_name != factories[index] ||
        component.state().attached_owner != source_handle(reviewed_source_index).value)
      throw std::runtime_error("External cut command reader attachment is unsupported");
  }
  const auto authored = resources_.sources().intro_external_cut_commands_source(reviewed_source_index);
  IntroExternalCutCommandsReaderState state{
      .owner = source_handle(reviewed_source_index), .resource = work.resource,
      .component_indices = {attachments[0], attachments[1]}, .commands = authored.commands,
      .external_list_resources = {}};
  for (std::size_t index = 0; index < authored.external_list_references.size(); ++index) {
    const auto target = resources_.sources().local_source_for_authored_reference(
        authored.external_list_references[index]);
    if (!target || *target >= directory_resource_mapping_.size() ||
        !directory_resource_mapping_[*target])
      throw std::runtime_error("External cut command reader has an unresolved external list reference");
    state.external_list_resources[index] = *directory_resource_mapping_[*target];
  }
  external_cut_commands_reader_state_ = std::move(state);
  try { record_supported_reader_admission(work); }
  catch (...) { external_cut_commands_reader_state_.reset(); throw; }
}

void IntroRuntime::apply_supported_first_cut_fade_picture_deferred_reader(
    const IntroDeferredReaderWork& work) {
  if (resource_load_stage_ != IntroResourceLoadStage::directory_construction_complete ||
      !work.processed || work.source_directory_index >= resources_.sources().directory().size() ||
      std::ranges::find_if(deferred_reader_work_, [&](const auto &candidate) {
        return std::addressof(candidate) == std::addressof(work);
      }) == deferred_reader_work_.end())
    throw std::runtime_error("Fade picture reader requires live deferred owner work");
  const auto& sources = resources_.sources();
  bool first_cut_target = false;
  for (const auto& command : resources_.first_cut().commands) {
    const auto source = sources.local_source_for_authored_reference(command.target_reference);
    if (source && *source == work.source_directory_index) first_cut_target = true;
  }
  if (!first_cut_target || fade_picture_reader_states_.contains(work.source_directory_index))
    throw std::runtime_error("Fade picture reader requires an unconsumed first-cut target");
  const auto& source = sources.directory().at(work.source_directory_index);
  const auto attachments = owner_components(source_handle(work.source_directory_index));
  if (source.source_type != 0x00200046U || source.deferred_source_offset != work.source_offset ||
      directory_resource_mapping_.at(work.source_directory_index) != work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource) != source_handle(work.source_directory_index) ||
      source.attachments.size() != 1U || attachments.size() != 1U ||
      source.attachments[0].parameter != 0.0F ||
      sources.attachment_identifier(work.source_directory_index, 0) != "ZWINPIC_FadeToBlack")
    throw std::runtime_error("Fade picture reader source shape is unsupported");
  const auto& component = components_.at(attachments[0]);
  if (!component.constructed() || component.removed() ||
      component.source().factory_name != "ZWINPIC_FadeToBlack" ||
      component.state().attached_owner != source_handle(work.source_directory_index).value)
    throw std::runtime_error("Fade picture reader component is unavailable");
  const auto authored = sources.intro_fade_picture_source(work.source_directory_index);
  const auto picture = std::ranges::find_if(resources_.pictures(), [&](const auto& candidate) {
    return candidate.directory_index == work.source_directory_index &&
           candidate.source.picture_asset_reference == authored.picture_asset_reference;
  });
  if (picture == resources_.pictures().end())
    throw std::runtime_error("Fade picture reader has no retained prepared picture");
  static_cast<void>(picture_for_source(work.source_directory_index));
  const auto [entry, inserted] = fade_picture_reader_states_.emplace(work.source_directory_index,
      IntroFadePictureReaderState{source_handle(work.source_directory_index), work.resource,
          attachments[0], authored, authored.picture_asset_reference});
  if (!inserted) throw std::runtime_error("Fade picture reader cannot run twice");
  try { record_supported_reader_admission(work); }
  catch (...) { fade_picture_reader_states_.erase(entry); throw; }
}

void IntroRuntime::apply_supported_first_cut_legal_picture_deferred_reader(
    const IntroDeferredReaderWork& work) {
  if(resource_load_stage_!=IntroResourceLoadStage::directory_construction_complete ||
      !work.processed || legal_picture_reader_state_ ||
      work.source_directory_index>=resources_.sources().directory().size() ||
      std::ranges::find_if(deferred_reader_work_,[&](const auto& candidate) {
        return std::addressof(candidate)==std::addressof(work);
      })==deferred_reader_work_.end())
    throw std::runtime_error("Legal picture reader requires unconsumed live deferred work");
  const auto& sources=resources_.sources();
  const auto legal=sources.local_source_for_authored_reference(resources_.member().references[1]);
  if(!legal || *legal!=work.source_directory_index)
    throw std::runtime_error("Legal picture reader requires the first-cut legal source");
  const auto& source=sources.directory().at(*legal);
  const auto attachments=owner_components(source_handle(*legal));
  if(source.source_type!=0x00200046U || source.deferred_source_offset!=work.source_offset ||
      directory_resource_mapping_.at(*legal)!=work.resource ||
      !associated_resource_owner(work.resource) ||
      *associated_resource_owner(work.resource)!=source_handle(*legal) ||
      source.attachments.size()!=1U || attachments.size()!=1U ||
      std::bit_cast<std::uint32_t>(source.attachments[0].parameter)!=
          std::bit_cast<std::uint32_t>(1.0F) ||
      sources.attachment_identifier(*legal,0)!="ZGEOM_Center")
    throw std::runtime_error("Legal picture reader source shape is unsupported");
  const auto& component=components_.at(attachments[0]);
  if(!component.constructed() || component.removed() ||
      component.source().factory_name!="ZGEOM_Center" ||
      component.state().attached_owner!=source_handle(*legal).value)
    throw std::runtime_error("Legal picture reader component is unavailable");
  const auto authored=sources.intro_legal_picture_source(*legal);
  const auto picture=std::ranges::find_if(resources_.pictures(),[&](const auto& candidate) {
    return candidate.directory_index==*legal &&
        candidate.source.picture_asset_reference==authored.picture_asset_reference;
  });
  if(picture==resources_.pictures().end())
    throw std::runtime_error("Legal picture reader has no retained prepared picture");
  static_cast<void>(picture_for_source(*legal));
  legal_picture_reader_state_={source_handle(*legal),work.resource,attachments[0],authored,
                               authored.picture_asset_reference};
  try { record_supported_reader_admission(work); }
  catch(...) { legal_picture_reader_state_.reset(); throw; }
}

IntroRuntimeHandle IntroRuntime::source_handle(std::size_t source) const {
  if (source >= resources_.sources().directory().size()) throw std::runtime_error("intro source index is out of range");
  return {owner_base_+static_cast<std::uint64_t>(source)+1};
}
std::optional<std::size_t> IntroRuntime::source_index(IntroRuntimeHandle handle) const {
  static_cast<void>(hierarchy_index(handle));
  if (handle == root_handle() || default_camera_==handle) return std::nullopt;
  return static_cast<std::size_t>(handle.value-owner_base_-1);
}
std::uint32_t IntroRuntime::hierarchy_index(IntroRuntimeHandle handle) const {
  const auto found=owner_indices_.find(handle.value);
  if (found==owner_indices_.end())
    throw std::runtime_error("intro runtime handle is not live");
  return found->second;
}
IntroRuntimeResourceHandle IntroRuntime::resource_handle(IntroRuntimeHandle owner) const {
  const auto index=hierarchy_index(owner);
  if(resource_owners_[index]!=owner || !hierarchy_resources_[index])
    throw std::runtime_error("Owner has no associated live resource");
  return *hierarchy_resources_[index];
}
IntroRuntimeHandle IntroRuntime::resource_owner(IntroRuntimeResourceHandle resource) const {
  const auto owner=associated_resource_owner(resource);
  if(!owner) throw std::runtime_error("Resource has no associated live owner");
  return *owner;
}
std::uint32_t IntroRuntime::resource_index(IntroRuntimeResourceHandle resource) const {
  const auto found=resource_indices_.find(resource.value);
  if(found==resource_indices_.end()) throw std::runtime_error("Resource handle is not live");
  return found->second;
}
std::optional<IntroRuntimeHandle> IntroRuntime::associated_resource_owner(IntroRuntimeResourceHandle resource) const {
  return resource_owners_.at(resource_index(resource));
}
const std::optional<IntroRuntimeResourceState>& IntroRuntime::resource_state_for_handle(IntroRuntimeResourceHandle resource) const {
  return resource_states_.at(resource_index(resource));
}
IntroRuntimeResourceHandle IntroRuntime::resource_parent(IntroRuntimeHandle owner) const {
  const auto parent=hierarchy_.at(hierarchy_index(owner)).parent;
  if(parent==no_picture_transform_parent) return {};
  const auto resource=hierarchy_resources_.at(parent);
  if(!resource) throw std::runtime_error("Parent resource has not been allocated");
  return *resource;
}
std::vector<IntroRuntimeHandle> IntroRuntime::child_owners(IntroRuntimeHandle owner) const {
  const auto parent=hierarchy_index(owner);std::vector<IntroRuntimeHandle> children;
  for(std::size_t i=0;i<hierarchy_.size();++i)
    if(hierarchy_[i].parent==parent) children.push_back(hierarchy_owners_[i]);
  return children;
}
const std::optional<IntroRuntimeResourceState>& IntroRuntime::resource_state(IntroRuntimeHandle owner) const {
  return resource_states_.at(hierarchy_index(owner));
}
void IntroRuntime::assign_resource_state(IntroRuntimeHandle owner,IntroRuntimeResourceState state) {
  if(resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Resource construction previously failed");
  const auto index=hierarchy_index(owner);
  if(!hierarchy_resources_.at(index)) throw std::runtime_error("Resource has not been allocated");
  if(state.context.value) static_cast<void>(resource_index(state.context));
  resource_states_.at(index)=state;
}
void IntroRuntime::mutate_resource_low_byte(IntroRuntimeResourceHandle resource,
    std::uint32_t set_mask,std::uint32_t clear_mask) {
  if(default_camera_busy_ || default_camera_failed_ || resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Resource mutation unavailable during failed or active Default camera construction");
  const auto index=resource_index(resource);
  if(!resource_states_.at(index)) throw std::runtime_error("Resource flag word is unknown");
  std::vector<std::uint32_t> ancestors;
  if(set_mask&0xffU) {
    auto parent=hierarchy_.at(index).parent;
    while(parent!=no_picture_transform_parent) {
      if(parent==index || std::find(ancestors.begin(),ancestors.end(),parent)!=ancestors.end())
        throw std::runtime_error("Resource parent chain is cyclic");
      if(!resource_states_.at(parent)) throw std::runtime_error("Ancestor resource flag word is unknown");
      ancestors.push_back(parent);
      parent=hierarchy_.at(parent).parent;
    }
  }
  auto& flags=resource_states_[index]->flags;
  flags=(flags&0xffffff00U)|(((flags&~clear_mask)|set_mask)&0xffU);
  for(const auto parent:ancestors) resource_states_[parent]->flags|=set_mask&0xffU;
}
void IntroRuntime::set_resource_flags_no_maintenance(IntroRuntimeResourceHandle resource,
    std::uint32_t set_mask,std::uint32_t clear_mask,
    ResourceMutationModes modes) {
  if(default_camera_busy_ || default_camera_failed_ || resource_load_stage_==IntroResourceLoadStage::failed)
    throw std::runtime_error("Resource mutation unavailable during failed or active Default camera construction");
  const auto index=resource_index(resource);
  if(!resource_states_.at(index)) throw std::runtime_error("Resource flag word is unknown");
  const auto current=resource_states_[index]->flags;
  if((((current&~clear_mask)|set_mask)^current)&0x2000U)
    throw std::runtime_error("Resource 0x2000 transition requires unimplemented registration services");
  if((set_mask|clear_mask)&0x8000U) {
    if(modes.allocation_enabled && !modes.maintenance_suppressed)
      throw std::runtime_error("Active resource 0x8000 maintenance is unsupported");
  }
  if((set_mask|clear_mask)&0xffU) mutate_resource_low_byte(resource,set_mask,clear_mask);
  auto& flags=resource_states_[index]->flags;
  flags=(flags&~(clear_mask&0xffffff00U))|(set_mask&0xffffff00U);
}
PreviewCameraResourceView IntroRuntime::camera_resource_view(IntroRuntimeHandle owner) {
  static_cast<void>(camera_for_owner(owner));
  if(default_camera_==owner && (default_camera_busy_ || default_camera_failed_))
    throw std::runtime_error("Default camera is not available for ordinary update");
  const auto index=hierarchy_index(owner);auto& state=resource_states_.at(index);
  if(!state) throw std::runtime_error("Camera resource state has not been produced by the loader");
  auto& node=hierarchy_[index];return {node.matrix,node.position,state->flags};
}
std::optional<IntroRuntimeHandle> IntroRuntime::create_default_camera_resource(
    bool single_allocation_mode,const std::function<void(IntroRuntimeResourceHandle)>& enqueue_transform) {
  if(default_camera_busy_ || default_camera_failed_)
    throw std::runtime_error("Default camera construction reentry or prior failure");
  if(registered_cameras_.camera_at(0,[this](std::uint64_t owner){return live_owner(owner);}))
    return std::nullopt;
  const bool ordinary_tail=(resource_load_stage_==IntroResourceLoadStage::directory_construction_complete &&
      reader_bracket_stage_==IntroReaderBracketStage::ordinary_reader_boundary_complete);
  if(resource_load_stage_!=IntroResourceLoadStage::prepared && !ordinary_tail)
    throw std::runtime_error("Authored resource loading must complete after ROOT construction before DefaultCam fallback");
  if(default_camera_) throw std::runtime_error("Default camera already awaits component admission");
  if(components_.phases_completed() || components_.failed())
    throw std::runtime_error("Default camera creation belongs before global component initialization");
  const auto root_state=resource_states_.at(0);
  if(!root_state || !enqueue_transform)
    throw std::runtime_error("Default camera needs actual root resource state and transform queue");
  if(root_state->context.value) static_cast<void>(resource_index(root_state->context));
  if(hierarchy_.size()>=std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("Intro dynamic hierarchy capacity exhausted");
  const auto index=static_cast<std::uint32_t>(hierarchy_.size());
  const auto capacity=hierarchy_.size()+1;
  hierarchy_.reserve(capacity);hierarchy_owners_.reserve(capacity);
  resource_states_.reserve(capacity);owner_components_.reserve(capacity);
  hierarchy_resources_.reserve(capacity);resource_owners_.reserve(capacity);
  auto camera=std::make_unique<FreshIntroCamera>();
  IntroSynthesizedCameraMetadata metadata{"DefaultCam",0x400003U};
  const IntroRuntimeHandle owner{application_.allocate_runtime_owners(1)};
  owner_indices_.emplace(owner.value,index);
  try {resource_indices_.emplace(owner.value,index);}
  catch(...) {owner_indices_.erase(owner.value);throw;}
  hierarchy_.push_back({engine_identity,{0,0,0},0});
  hierarchy_owners_.push_back(owner);owner_components_.emplace_back();
  IntroRuntimeResourceState child{single_allocation_mode?0x01100000U:0x09000000U,{}};
  child.flags|=root_state->flags&0xc00U;
  if(root_state->flags&0x40040000U) {
    child.flags|=0x40000000U;
    child.context=(root_state->flags&0x40000U)?resource_handle(root_handle()):root_state->context;
  }
  resource_states_.push_back(child);
  hierarchy_resources_.push_back(IntroRuntimeResourceHandle{owner.value});
  resource_owners_.push_back(owner);
  default_camera_owner_=std::move(camera);default_camera_=owner;default_camera_metadata_=std::move(metadata);
  default_camera_context_=root_handle();
  struct Guard {bool& busy;~Guard(){busy=false;}} guard{default_camera_busy_};
  default_camera_busy_=true;
  try {
    default_camera_owner_->enable_preview_flag();
    // Fresh identity/+0 construction makes this the setter's changed path.
    auto& node=hierarchy_[index];auto& flags=resource_states_[index]->flags;
    node.position={0,50,-200};flags|=0x100000U;
    node.matrix=engine_identity;flags|=0x100000U;
    enqueue_transform(resource_handle(owner));
    return owner;
  }catch(...){default_camera_failed_=true;throw;}
}
void IntroRuntime::attach_default_preview_camera() {
  if(!default_camera_ || default_camera_busy_ || default_camera_failed_ || default_preview_component_ ||
      components_.phases_completed())
    throw std::runtime_error("Default PreviewCamera attachment is out of loader order");
  const auto owner=*default_camera_;
  if(!resource_state(owner)) throw std::runtime_error("Default camera resource state is unknown");
  default_camera_attachments_.reserve(1);
  auto& catalog=owner_components_.at(hierarchy_index(owner));catalog.reserve(catalog.size()+1);
  const auto index=components_.append({owner.value,std::nullopt,std::nullopt,"ZCAMERA_PreviewCamera",0,0,0,true});
  catalog.push_back(index);default_preview_component_=index;
  struct Guard {bool& busy;~Guard(){busy=false;}} guard{default_camera_busy_};default_camera_busy_=true;
  try {
    components_.construct(index,[&](runtime::ComponentRecord& record){
      auto payload=std::make_shared<PreviewCameraComponent>(application_.live_variables());
      default_preview_=payload;
      auto& state=record.state(); // Mutate the same common instance; preserve completed prefixes.
      state.class_ordinal=152;state.priority=0;state.requested=0x111;state.status|=0x20;
      state.attached_owner=owner.value;state.script_reference=0;
      default_camera_attachments_.push_back(index);
      if(!(resource_state(owner)->flags&0x400U)) {
        default_component_mask_|=state.requested;
        const auto added=state.requested&~state.admitted&0x158U;
        state.requested|=added;state.admitted|=added;
        if(added&0x10U) register_ordinary_component(index);
      }
      return runtime::ConstructedComponent{state,[payload](auto&){},[payload](auto&){}};
    });
  }catch(...){default_camera_failed_=true;throw;}
}
void IntroRuntime::finish_default_camera_registration(const IntroCameraRegistrationServices& services) {
  if(!default_camera_ || !default_preview_component_ || !default_preview_ || default_camera_failed_ || default_camera_busy_ ||
      default_camera_registered_ || components_.phases_completed() || !components_.at(*default_preview_component_).constructed())
    throw std::runtime_error("Default camera requires successful PreviewCamera attachment");
  try {
    default_camera_owner_->set_priority(0x40000000);
    register_camera(*default_camera_,0.0F,services);
    default_camera_registered_=true;
  }catch(...){default_camera_failed_=true;throw;}
}
std::optional<IntroRuntimeHandle> IntroRuntime::ensure_default_camera(bool single_allocation_mode,
    const std::function<void(IntroRuntimeResourceHandle)>& enqueue_transform,
    const IntroCameraRegistrationServices& registration) {
  const auto owner=create_default_camera_resource(single_allocation_mode,enqueue_transform);
  if(!owner) return std::nullopt;
  attach_default_preview_camera();finish_default_camera_registration(registration);return owner;
}
void IntroRuntime::run_ordinary_components(const IntroOrdinaryFrameServices& services) {
  if(!components_.phases_completed() || components_.failed() || default_camera_failed_ || default_camera_busy_)
    throw std::runtime_error("Ordinary intro update requires complete live component initialization");
  if(!ordinary_) return;
  runtime::OrdinaryDispatchServices dispatch;
  dispatch.scene_integer=[this]{return application_.clock().scene_integer_word();};
  dispatch.assign_dispatch_time=[this](auto value){application_.assign_component_dispatch_time(value);};
  dispatch.paused=services.paused;dispatch.filter=services.component_filter;
  dispatch.phase_one_diagnostic=[](auto&){throw std::runtime_error("Ordinary component phase one is incomplete");};
  dispatch.direct_event16=[this,services](runtime::ComponentRecord& record){
    if(!default_preview_component_ || &record!=&components_.at(*default_preview_component_) || !default_preview_)
      throw std::runtime_error("Unsupported intro ordinary component: "+record.source().factory_name);
    if(record.state().attached_owner!=default_camera_->value || !services.preview_input || !services.enqueue_transform)
      throw std::runtime_error("PreviewCamera needs its live typed owner, real input and transform queue");
    const auto owner=*default_camera_;
    default_preview_->update(application_,camera_for_owner(owner),camera_resource_view(owner),services.preview_input(),
      [this,services,owner]{services.enqueue_transform(resource_handle(owner));});
  };
  dispatch.retire=[](auto&,auto){throw std::runtime_error("Intro ordinary component retirement requires concrete teardown");};
  ordinary_->dispatch(dispatch);
}
void IntroRuntime::set_local_transform(IntroRuntimeHandle handle,
    const std::array<float,9>& basis,const std::array<float,3>& position) {
  const auto index = hierarchy_index(handle);
  for (float value : basis) if (!std::isfinite(value)) throw std::runtime_error("intro basis must be finite");
  for (float value : position) if (!std::isfinite(value)) throw std::runtime_error("intro position must be finite");
  hierarchy_[index].matrix = basis;
  hierarchy_[index].position = position;
  for (auto& picture : pictures_) picture->cache_.invalidate();
}
IntroRuntimePicture& IntroRuntime::picture_for_source(std::size_t source) {
  for (auto& picture : pictures_) if (picture->source_index() == source) return *picture;
  throw std::runtime_error("intro source has no retained picture owner");
}
const IntroRuntimePicture& IntroRuntime::picture_for_source(std::size_t source) const {
  for (const auto& picture : pictures_) if (picture->source_index() == source) return *picture;
  throw std::runtime_error("intro source has no retained picture owner");
}
std::uint32_t IntroRuntime::paired_material(std::uint32_t prm_offset) const {
  const auto found = materials_.find(prm_offset);
  if (found == materials_.end()) throw std::runtime_error("intro paired frame resource is not retained");
  return found->second;
}
void IntroRuntime::project_selected_window_camera_state() {
  if (projected_) throw std::runtime_error("fresh window camera projection cannot run twice");
  const auto& window = resources_.window();
  // The bounded preservation result requires the unchanged authored orientation
  // and null auxiliary cursor. Do not invoke or erase the other lifecycle work.
  if (window.opaque_references[0] != 0 || window.opaque_references[1] != 0)
    throw std::runtime_error("window camera projection requires null auxiliary references");
  const auto& basis = hierarchy_.at(hierarchy_index(source_handle(resources_.window_index()))).matrix;
  for (std::size_t i=0; i<basis.size(); ++i)
    if (std::bit_cast<std::uint32_t>(basis[i]) != std::bit_cast<std::uint32_t>(engine_identity[i]))
      throw std::runtime_error("window camera projection requires unchanged engine identity orientation");
  camera().apply_window_state_projection(window.options[0] != 0,window.options[1] != 0,
      source_handle(resources_.window_index()).value);
  projected_ = true;
}
} // namespace off::graphics
