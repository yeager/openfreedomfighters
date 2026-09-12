#pragma once

#include "off/data/gms_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace off::cutscene {

struct FirstCutPlayerDescriptor {
  std::size_t list_component{};
  std::array<std::size_t, 5> command_components{};
  data::GmsIntroFirstCutSource list;
  data::GmsIntroCutSequenceSource sequence;
};

struct FirstCutPlayerPhaseOneServices {
  std::function<void(std::size_t, const data::GmsIntroCutCommandSource&)> invoke_command;
  std::function<void()> read_retained_source;
  std::function<void()> register_list_events;
  std::function<std::size_t()> member_count;
  std::function<void(std::uint64_t)> write_queue_property;
  std::function<void()> setup_action_map;
  std::uint64_t queue_property_value{};
};

struct FirstCutMemberInfo { float end{}; };
struct FirstCutPlayerPhaseTwoServices {
  std::function<void(std::size_t, const data::GmsIntroCutCommandSource&)> invoke_command;
  std::function<std::optional<std::uint64_t>(std::string_view)> read_scene_reference;
  std::function<std::optional<std::uint64_t>(std::size_t)> resolve_member;
  std::function<std::optional<FirstCutMemberInfo>(std::uint64_t)> request_member_info;
  std::function<std::string_view(std::uint64_t)> member_name;
  std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_scene_object;
  std::function<void(std::size_t, const data::GmsIntroCutCommandSource&)> register_ordered_command;
  std::function<void(std::size_t)> close_ordered_command_receiver;
};
struct FirstCutPlayerSessionPhaseTwoServices {
  std::function<void(std::size_t, const data::GmsIntroCutCommandSource&)> invoke_command;
  std::function<std::optional<std::uint64_t>(std::string_view)> read_scene_reference;
  std::function<std::optional<std::uint64_t>(std::size_t)> resolve_member;
  std::function<std::optional<FirstCutMemberInfo>(std::uint64_t)> request_member_info;
  std::function<std::string_view(std::uint64_t)> member_name;
  std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_scene_object;
};

// Concrete ordered-command receiver for the reviewed first-cut list. It is
// intentionally separate from the source-65 external-fade list model.
class FirstCutPlayerListReceiver final {
public:
  explicit FirstCutPlayerListReceiver(std::size_t list_component) : list_component_(list_component) {}
  void open_after_phase_one(std::size_t live_list_component);
  void register_ordered_command(std::size_t live_list_component,
                                const data::GmsIntroCutCommandSource& command);
  void close_after_phase_two(std::size_t live_list_component);
  [[nodiscard]] const std::vector<data::GmsIntroCutCommandSource>& commands() const noexcept { return commands_; }
  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] bool closed() const noexcept { return closed_; }

private:
  [[nodiscard]] float key_at(std::size_t index) const noexcept;
  std::size_t list_component_{};
  std::vector<data::GmsIntroCutCommandSource> commands_;
  std::optional<std::size_t> cached_command_;
  bool open_{}, closed_{};
};

// One reviewed first-cut list initialization. It retains data and executes
// only explicit lifecycle callbacks; it never samples clocks or starts a cut.
class FirstCutPlayerInitialization final {
public:
  explicit FirstCutPlayerInitialization(FirstCutPlayerDescriptor descriptor);
  void run_phase_one(const FirstCutPlayerPhaseOneServices& services);
  void run_phase_two(const FirstCutPlayerPhaseTwoServices& services);

  [[nodiscard]] bool phase_one_complete() const noexcept { return phase_one_complete_; }
  [[nodiscard]] bool phase_two_complete() const noexcept { return phase_two_complete_; }
  [[nodiscard]] bool source_read_marker() const noexcept { return source_read_marker_; }
  [[nodiscard]] bool events_registered() const noexcept { return events_registered_; }
  [[nodiscard]] std::size_t list_component() const noexcept { return list_component_; }
  [[nodiscard]] const std::vector<bool>& started() const noexcept { return started_; }
  [[nodiscard]] const std::vector<bool>& completed() const noexcept { return completed_; }
  [[nodiscard]] float derived_end() const noexcept { return derived_end_; }
  [[nodiscard]] std::optional<std::uint64_t> active_camera_list() const noexcept { return active_camera_list_; }
  [[nodiscard]] std::optional<std::uint64_t> cut_sequence_object() const noexcept { return cut_sequence_object_; }
  // A lifecycle receiver may only be joined to the exact cold descriptor that
  // was projected from the currently live runtime. Component indices alone
  // are not an identity: a caller could otherwise substitute a different
  // source payload with the same list component.
  [[nodiscard]] bool has_same_descriptor_as(
      const FirstCutPlayerInitialization& other) const;

 private:
  data::GmsIntroFirstCutSource list_;
  data::GmsIntroCutSequenceSource sequence_;
  std::size_t list_component_{};
  std::array<std::size_t, 5> command_components_{};
  std::vector<bool> started_, completed_;
  float derived_end_{};
  std::optional<std::uint64_t> active_camera_list_, cut_sequence_object_;
  bool source_read_marker_{true}, events_registered_{}, phase_one_complete_{}, phase_two_complete_{}, running_{}, failed_{};
};

// Owns one reviewed first-cut initialization and its exact list receiver.
// This composes lifecycle phases only; it does not schedule, play, or render.
class FirstCutPlayerSession final {
public:
  explicit FirstCutPlayerSession(FirstCutPlayerDescriptor descriptor);
  void run_phase_one(const FirstCutPlayerPhaseOneServices& services);
  void run_phase_two(const FirstCutPlayerSessionPhaseTwoServices& services);
  [[nodiscard]] const FirstCutPlayerInitialization& initialization() const noexcept { return initialization_; }
  [[nodiscard]] const FirstCutPlayerListReceiver& receiver() const noexcept { return receiver_; }

private:
  FirstCutPlayerInitialization initialization_;
  FirstCutPlayerListReceiver receiver_;
};

// A strictly cold adapter for exercising the two recovered initialization
// phases against retained, caller-provided live identities. It records only
// aggregate lifecycle evidence; it does not schedule a cut or invoke a host.
struct FirstCutPlayerInitializationObservationBindings {
  std::uint64_t active_camera_list{};
  std::uint64_t cut_sequence_object{};
  std::uint64_t member{};
  float member_end{};
  std::size_t member_count{};
  std::uint64_t queue_property{};
};

struct FirstCutPlayerInitializationObservation {
  std::size_t phase_one_command_invocations{};
  std::size_t phase_two_command_invocations{};
  std::size_t ordered_command_registrations{};
  bool retained_source_read{};
  bool list_events_registered{};
  bool queue_property_written{};
  bool action_map_setup{};
  bool receiver_open{};
  bool receiver_closed{};
  bool active_camera_list_resolved{};
  bool cut_sequence_object_resolved{};
  float derived_end{};
};

[[nodiscard]] FirstCutPlayerInitializationObservation
observe_first_cut_player_initialization(
    FirstCutPlayerSession& session,
    const FirstCutPlayerInitializationObservationBindings& bindings);

} // namespace off::cutscene
