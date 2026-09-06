#include "off/graphics/intro_runtime.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
constexpr std::array<float,9> engine_identity{0,0,1,0,1,0,1,0,0};
}

audio::SoundRecord& IntroRuntimeSound::record() {
  if (failed_) throw std::runtime_error("Intro sound owner is failed");
  return lease_.get();
}
const audio::SoundRecord& IntroRuntimeSound::record() const {
  if (failed_) throw std::runtime_error("Intro sound owner is failed");
  return lease_.get();
}

IntroRuntime::IntroRuntime(IntroPreparedResources&& resources, runtime::ApplicationServices& application,
                           runtime::SceneComponentSequence& component_sequence)
    : application_(application), resources_(std::move(resources)), components_(component_sequence),
      camera_(resources_.camera()) {
  const auto& directory = resources_.sources().directory();
  if (directory.size() >= std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("intro hierarchy exceeds native index capacity");
  owner_base_=application_.allocate_runtime_owners(directory.size()+1);
  camera_context_=root_handle();
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
  for(std::size_t index=0;index<hierarchy_.size();++index) {
    const IntroRuntimeHandle owner{owner_base_+index};
    hierarchy_owners_.push_back(owner);
    owner_indices_.emplace(owner.value,static_cast<std::uint32_t>(index));
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
  // Normal, non-restore source loading under the explicit native logical-backend
  // policy. This allocates records but neither publishes owner bindings nor
  // runs owner/component initialization, opens a device, or starts a channel.
  sounds_.reserve(resources_.sounds().size());
  for (const auto& source : resources_.sounds()) {
    auto owner = std::make_unique<IntroRuntimeSound>();
    owner->source_ = &source;
    owner->handle_ = source_handle(source.directory_index);
    owner->lease_ = application_.sound_records().create(owner->handle_.value);
    application_.sound_records().apply_source(owner->lease_.get(),source.source);
    sounds_.push_back(std::move(owner));
  }
}

IntroRuntimeSound& IntroRuntime::sound_for_source(std::size_t source) {
  for (auto& sound : sounds_) if (sound->source_index() == source) return *sound;
  throw std::runtime_error("Intro source has no retained sound owner");
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
  if(owner==source_handle(resources_.camera_index())) return camera_;
  if(default_camera_==owner) return *default_camera_owner_;
  throw std::runtime_error("Intro owner is not a constructed camera");
}
const FreshIntroCamera& IntroRuntime::camera_for_owner(IntroRuntimeHandle owner) const {
  static_cast<void>(hierarchy_index(owner));
  if(owner==source_handle(resources_.camera_index())) return camera_;
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
  else camera_context_=context;
}
IntroRuntimeHandle IntroRuntime::camera_context(IntroRuntimeHandle owner) const {
  static_cast<void>(camera_for_owner(owner));
  return default_camera_==owner?default_camera_context_:camera_context_;
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
  static_cast<void>(hierarchy_index(owner));return {owner.value};
}
IntroRuntimeHandle IntroRuntime::resource_owner(IntroRuntimeResourceHandle resource) const {
  const IntroRuntimeHandle owner{resource.value};static_cast<void>(hierarchy_index(owner));return owner;
}
IntroRuntimeResourceHandle IntroRuntime::resource_parent(IntroRuntimeHandle owner) const {
  const auto parent=hierarchy_.at(hierarchy_index(owner)).parent;
  return parent==no_picture_transform_parent?IntroRuntimeResourceHandle{}:
    resource_handle(hierarchy_owners_.at(parent));
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
  const auto index=hierarchy_index(owner);
  if(state.context.value) static_cast<void>(resource_owner(state.context));
  resource_states_.at(index)=state;
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
  if(default_camera_) throw std::runtime_error("Default camera already awaits component admission");
  if(components_.phases_completed() || components_.failed())
    throw std::runtime_error("Default camera creation belongs before global component initialization");
  const auto root_state=resource_states_.at(0);
  if(!root_state || !enqueue_transform)
    throw std::runtime_error("Default camera needs actual root resource state and transform queue");
  if(root_state->context.value) static_cast<void>(resource_owner(root_state->context));
  if(hierarchy_.size()>=std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error("Intro dynamic hierarchy capacity exhausted");
  const auto index=static_cast<std::uint32_t>(hierarchy_.size());
  const auto capacity=hierarchy_.size()+1;
  hierarchy_.reserve(capacity);hierarchy_owners_.reserve(capacity);
  resource_states_.reserve(capacity);owner_components_.reserve(capacity);
  auto camera=std::make_unique<FreshIntroCamera>();
  IntroSynthesizedCameraMetadata metadata{"DefaultCam",0x400003U};
  const IntroRuntimeHandle owner{application_.allocate_runtime_owners(1)};
  owner_indices_.emplace(owner.value,index);
  hierarchy_.push_back({engine_identity,{0,0,0},0});
  hierarchy_owners_.push_back(owner);owner_components_.emplace_back();
  IntroRuntimeResourceState child{single_allocation_mode?0x01100000U:0x09000000U,{}};
  child.flags|=root_state->flags&0xc00U;
  if(root_state->flags&0x40040000U) {
    child.flags|=0x40000000U;
    child.context=(root_state->flags&0x40000U)?resource_handle(root_handle()):root_state->context;
  }
  resource_states_.push_back(child);
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
  camera_.apply_window_state_projection(window.options[0] != 0,window.options[1] != 0,
      source_handle(resources_.window_index()).value);
  projected_ = true;
}
} // namespace off::graphics
