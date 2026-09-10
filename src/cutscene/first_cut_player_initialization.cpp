#include "off/cutscene/first_cut_player_initialization.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace off::cutscene {
float FirstCutPlayerListReceiver::key_at(std::size_t index) const noexcept {
  return static_cast<float>(std::bit_cast<std::int32_t>(commands_[index].timeline_position));
}
void FirstCutPlayerListReceiver::open_after_phase_one(std::size_t live_list_component) {
  if (open_ || closed_ || live_list_component != list_component_)
    throw std::runtime_error("first-cut list receiver phase-one boundary is invalid");
  open_ = true;
}
void FirstCutPlayerListReceiver::register_ordered_command(
    std::size_t live_list_component, const data::GmsIntroCutCommandSource& command) {
  if (!open_ || closed_ || live_list_component != list_component_ ||
      std::bit_cast<std::int32_t>(command.timeline_position) < 0)
    throw std::runtime_error("first-cut list receiver command admission is invalid");
  const auto key = static_cast<float>(std::bit_cast<std::int32_t>(command.timeline_position));
  std::size_t insertion{};
  if (cached_command_) {
    insertion = *cached_command_;
    while (key_at(insertion) > key) { if (insertion == 0U) break; --insertion; }
    while (insertion < commands_.size() && key > key_at(insertion)) ++insertion;
  }
  commands_.insert(commands_.begin() + static_cast<std::ptrdiff_t>(insertion), command);
  cached_command_ = insertion;
}
void FirstCutPlayerListReceiver::close_after_phase_two(std::size_t live_list_component) {
  if (!open_ || closed_ || live_list_component != list_component_)
    throw std::runtime_error("first-cut list receiver phase-two boundary is invalid");
  closed_ = true;
}
FirstCutPlayerInitialization::FirstCutPlayerInitialization(FirstCutPlayerDescriptor descriptor)
    : list_(std::move(descriptor.list)), sequence_(std::move(descriptor.sequence)),
      list_component_(descriptor.list_component), command_components_(descriptor.command_components) {
  if (std::ranges::any_of(command_components_, [this](auto value) { return value == list_component_; }))
    throw std::runtime_error("first-cut player component roles overlap");
  if (!std::isfinite(list_.final_value) || !std::isfinite(sequence_.values[0]) ||
      !std::isfinite(sequence_.values[1]))
    throw std::runtime_error("first-cut player source requires finite values");
}
void FirstCutPlayerInitialization::run_phase_one(const FirstCutPlayerPhaseOneServices& services) {
  if (phase_one_complete_ || phase_two_complete_ || running_ || failed_ || !services.invoke_command || !services.read_retained_source ||
      !services.register_list_events || !services.member_count || !services.write_queue_property)
    throw std::runtime_error("first-cut player phase one boundary is invalid");
  running_ = true;
  try {
    for (std::size_t index = command_components_.size(); index-- > 0U;)
      services.invoke_command(command_components_[index], list_.commands[index]);
    services.read_retained_source();
    services.register_list_events();
    const auto count = services.member_count();
    std::vector<bool> started(count, false), completed(count, false);
    services.write_queue_property(services.queue_property_value);
    if (services.setup_action_map) services.setup_action_map();
    started_ = std::move(started); completed_ = std::move(completed);
    source_read_marker_ = false; events_registered_ = true; phase_one_complete_ = true;
    running_ = false;
  } catch (...) { running_ = false; failed_ = true; throw; }
}
void FirstCutPlayerInitialization::run_phase_two(const FirstCutPlayerPhaseTwoServices& services) {
  if (!phase_one_complete_ || phase_two_complete_ || running_ || failed_ || !services.invoke_command || !services.read_scene_reference ||
      !services.resolve_member || !services.request_member_info || !services.member_name ||
      !services.resolve_scene_object || !services.register_ordered_command || !services.close_ordered_command_receiver)
    throw std::runtime_error("first-cut player phase two boundary is invalid");
  running_ = true;
  try {
    for (std::size_t index = command_components_.size(); index-- > 0U;) {
      services.invoke_command(command_components_[index], list_.commands[index]);
      if (std::bit_cast<std::int32_t>(list_.commands[index].timeline_position) >= 0)
        services.register_ordered_command(list_component_, list_.commands[index]);
    }
    const auto active = services.read_scene_reference("rActiveCameraList");
    float derived = 0.0F;
    for (std::size_t index = 0; index < started_.size(); ++index) {
      const auto member = services.resolve_member(index);
      if (!member) continue;
      const auto info = services.request_member_info(*member);
      if (!info) continue;
      if (!std::isfinite(info->end)) throw std::runtime_error("first-cut member end is not finite");
      if (info->end > derived) derived = info->end;
      if (services.member_name(*member) == "Cut_Geom_List_02") derived = 0.0F;
    }
    const auto source = services.read_scene_reference("rCutSequenceObject");
    const auto object = source ? services.resolve_scene_object(*source) : std::optional<std::uint64_t>{};
    services.close_ordered_command_receiver(list_component_);
    active_camera_list_ = active; cut_sequence_object_ = object; derived_end_ = derived;
    phase_two_complete_ = true; running_ = false;
  } catch (...) { running_ = false; failed_ = true; throw; }
}
} // namespace off::cutscene
