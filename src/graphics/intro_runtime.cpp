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
  return {static_cast<std::uint64_t>(source)+2};
}
std::optional<std::size_t> IntroRuntime::source_index(IntroRuntimeHandle handle) const {
  static_cast<void>(hierarchy_index(handle));
  if (handle == root_handle()) return std::nullopt;
  return static_cast<std::size_t>(handle.value-2);
}
std::uint32_t IntroRuntime::hierarchy_index(IntroRuntimeHandle handle) const {
  if (handle.value == 0 || handle.value > resources_.sources().directory().size()+1)
    throw std::runtime_error("intro runtime handle is not live");
  return static_cast<std::uint32_t>(handle.value-1);
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
