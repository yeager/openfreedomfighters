#pragma once
#include "off/audio/sound_preferences.hpp"
#include "off/audio/sound_records.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include "off/graphics/preview_camera_update.hpp"
#include "off/runtime/application_clock.hpp"
#include "off/runtime/live_variables.hpp"
#include "off/runtime/input_maps.hpp"
#include "off/runtime/ordinary_components.hpp"
#include <utility>
#include <stdexcept>
#include <limits>
#include <map>
#include <string>
#include <string_view>

namespace off::runtime {
// Application-lifetime state, borrowed by scenes rather than reset when each
// IntroRuntime is constructed. No global numeric/scene-reference stores are
// inferred from this separate text configuration or from backend absence.
class ApplicationServices final {
public:
  ApplicationServices(ClockExecutionPolicy policy, ClockSamplingServices sampling,
                      std::function<audio::SoundVolumeBackend*()> live_backend = {})
    : sampling_(std::move(sampling)), clock_(policy),
      sound_({configuration_,live_backend ? std::move(live_backend) :
          std::function<audio::SoundVolumeBackend*()>{[this] { return &sound_records_; }}}) {}
  [[nodiscard]] ApplicationClock& clock() noexcept { return clock_; }
  [[nodiscard]] const ApplicationClock& clock() const noexcept { return clock_; }
  [[nodiscard]] audio::SoundPreferences& sound() noexcept { return sound_; }
  [[nodiscard]] graphics::PreviewCameraUpdate& preview_camera_update() noexcept { return preview_camera_update_; }
  [[nodiscard]] LiveVariableRegistry& live_variables() noexcept { return live_variables_; }
  [[nodiscard]] InputMapRegistry& input_maps() noexcept {return input_maps_;}
  // Explicit native registration of the implemented concrete ZGROUP factory.
  // This is not the original full registry preparation/base-class resolution.
  // Host creation/destruction does not reset or decrement the notification word.
  void initialize_native_group_registration() {
    if(!class_sequences_.emplace(0x00100001U,0U).second)
      throw std::runtime_error("Native group class is already registered");
  }
  void initialize_native_window_language_registration() {
    if(has_class_registration(0x00100030U) || has_class_registration(0x00101389U))
      throw std::runtime_error("Native window or language class is already registered");
    auto updated=class_sequences_;
    updated.emplace(0x00100030U,0U);
    updated.emplace(0x00101389U,0U);
    class_sequences_.swap(updated);
  }
  [[nodiscard]] bool has_class_registration(std::uint32_t identity) const noexcept {
    return class_sequences_.contains(identity);
  }
  void initialize_native_camera_registration() {
    if(!class_sequences_.emplace(0x00400003U,0U).second)
      throw std::runtime_error("Native camera class is already registered");
  }
  void initialize_native_visual_registration() {
    if(!class_sequences_.emplace(0x0020003aU,0U).second)
      throw std::runtime_error("Native visual class is already registered");
  }
  void initialize_native_picture_registration() {
    if(has_class_registration(0x00200046U) || has_component_class_registration("ZGEOM_Center") ||
        has_component_class_registration("ZWINPIC_FadeToBlack"))
      throw std::runtime_error("Native Picture or attachment class is already registered");
    auto owners=class_sequences_;
    auto components=component_class_sequences_;
    owners.emplace(0x00200046U,0U);
    components.emplace("ZGEOM_Center",0U);
    components.emplace("ZWINPIC_FadeToBlack",0U);
    class_sequences_.swap(owners);
    component_class_sequences_.swap(components);
  }
  [[nodiscard]] bool has_component_class_registration(std::string_view name) const {
    return component_class_sequences_.contains(name);
  }
  void initialize_native_second_window_scope_registration() {
    constexpr std::uint32_t owner_classes[]{0x0020002dU,0x0800001aU};
    constexpr std::string_view attachment_classes[]{
        "ZCHAROBJ_CharFader","ZWINPIC_LogoFade","ZLIST_ExternCutSequenceCommand"};
    for(const auto identity:owner_classes)
      if(has_class_registration(identity))
        throw std::runtime_error("Native second Window owner class is already registered");
    for(const auto name:attachment_classes)
      if(has_component_class_registration(name))
        throw std::runtime_error("Native second Window attachment class is already registered");
    auto owners=class_sequences_;
    auto components=component_class_sequences_;
    for(const auto identity:owner_classes) owners.emplace(identity,0U);
    for(const auto name:attachment_classes) components.emplace(name,0U);
    class_sequences_.swap(owners);
    component_class_sequences_.swap(components);
  }
  [[nodiscard]] std::uint32_t component_class_notification_sequence(std::string_view name) const {
    const auto found=component_class_sequences_.find(name);
    if(found==component_class_sequences_.end()) throw std::runtime_error("Component class is not registered");
    return found->second;
  }
  std::uint32_t register_component_class_instance(std::string_view name) {
    auto found=component_class_sequences_.find(name);
    if(found==component_class_sequences_.end()) throw std::runtime_error("Component class is not registered");
    const auto previous=found->second;
    ++found->second;
    return previous;
  }
  [[nodiscard]] std::uint32_t class_notification_sequence(std::uint32_t identity) const {
    const auto found=class_sequences_.find(identity);
    if(found==class_sequences_.end()) throw std::runtime_error("Concrete class is not registered");
    return found->second;
  }
  std::uint32_t register_class_instance(std::uint32_t identity) {
    const auto previous=class_notification_sequence(identity);
    class_sequences_.at(identity)=previous+std::uint32_t{1};
    return previous;
  }
  [[nodiscard]] bool has_group_registration() const noexcept {return has_class_registration(0x00100001U);}
  [[nodiscard]] std::uint32_t group_class_instance_count() const {
    return class_notification_sequence(0x00100001U);
  }
  std::uint32_t register_group_instance() {
    return register_class_instance(0x00100001U);
  }
  [[nodiscard]] OrdinarySortingState& ordinary_sorting() noexcept {return ordinary_sorting_;}
  void assign_component_dispatch_time(std::uint32_t time) noexcept {component_dispatch_time_=time;}
  [[nodiscard]] std::optional<std::uint32_t> component_dispatch_time() const noexcept {return component_dispatch_time_;}
  // Native application-scoped owner IDs never alias a previously destroyed
  // scene. This is not the original allocator or the component serial counter.
  [[nodiscard]] std::uint64_t allocate_runtime_owners(std::uint64_t count) {
    if(!count || !next_runtime_owner_ || count-1>std::numeric_limits<std::uint64_t>::max()-next_runtime_owner_)
      throw std::runtime_error("Native runtime owner identity domain exhausted");
    const auto first=next_runtime_owner_;
    next_runtime_owner_+=count;
    return first;
  }
  // Explicit component-update boundary. Input queries and the live camera/queue
  // come from the caller; pointer raw delta and keyboard last scaled increment
  // always come from this canonical clock (not frozen scene publication).
  void update_preview_camera(graphics::FreshIntroCamera& owner,graphics::PreviewCameraPose& camera,
      graphics::PreviewCameraInput input,
      const std::function<void(graphics::PreviewCameraPose&)>& enqueue_transform) {
    if (!clock_.ready() || clock_.failed() || !clock_.state().crt_mode)
      throw std::runtime_error("preview camera requires the live CRT application clock");
    input.raw_crt_delta=clock_.state().raw_delta;
    input.last_scaled_increment=clock_.state().last_scaled_increment;
    preview_camera_update_.run(owner,camera,input,enqueue_transform);
  }
  void update_preview_camera(graphics::FreshIntroCamera& owner,graphics::PreviewCameraResourceView resource,
      graphics::PreviewCameraInput input,const std::function<void()>& enqueue_transform) {
    if(!clock_.ready() || clock_.failed() || !clock_.state().crt_mode)
      throw std::runtime_error("preview camera requires the live CRT application clock");
    input.raw_crt_delta=clock_.state().raw_delta;
    input.last_scaled_increment=clock_.state().last_scaled_increment;
    preview_camera_update_.run(owner,resource,input,enqueue_transform);
  }
  // Canonical logical records survive across scene loads. This store alone is
  // not an output device or a producer of playback-start acknowledgements.
  [[nodiscard]] audio::SoundRecordRegistry& sound_records() noexcept { return sound_records_; }
  [[nodiscard]] const audio::SoundRecordRegistry& sound_records() const noexcept { return sound_records_; }
  [[nodiscard]] audio::SoundTextConfiguration& configuration() noexcept { return configuration_; }
  void reset_clock() { clock_.reset(sampling_); }
  void rebase_clock() { clock_.rebase(sampling_); }
  std::int32_t advance_crt() { return clock_.advance_crt(sampling_); }
  // Leaves actual input, global numeric property and renderer services intact.
  // Binding is transactional; this owner must outlive all resulting callbacks.
  void bind_controller_phase_two(graphics::IntroControllerPhaseTwoServices& supplied) {
    auto bound=supplied;
    sound_.bind_phase_two_services(bound);
    bound.assign_engine_clock_mode=[this](bool value) { clock_.assign_crt_mode(value); };
    bound.scene_integer_clock=[this] { return clock_.scene_integer_word(); };
    supplied=std::move(bound);
  }
private:
  ClockSamplingServices sampling_;
  ApplicationClock clock_;
  audio::SoundTextConfiguration configuration_;
  audio::SoundRecordRegistry sound_records_;
  audio::SoundPreferences sound_;
  graphics::PreviewCameraUpdate preview_camera_update_;
  LiveVariableRegistry live_variables_;
  InputMapRegistry input_maps_;
  std::map<std::uint32_t,std::uint32_t> class_sequences_;
  std::map<std::string,std::uint32_t,std::less<>> component_class_sequences_;
  OrdinarySortingState ordinary_sorting_;
  // Actual application-constructor producer; later ordinary passes update it.
  std::optional<std::uint32_t> component_dispatch_time_{0U};
  std::uint64_t next_runtime_owner_{1};
};
} // namespace off::runtime
