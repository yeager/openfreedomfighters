#pragma once

#include "off/cutscene/command_pass.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/runtime/intro_live_target_registry.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace off::cutscene {

// The required mutation boundary for one already-admitted first-cut command
// pass.  Name lookup is deliberately opt-in: an unavailable name resolver is
// represented as an unresolved target, never as a broader scene search.
struct FirstCutCommandSessionServices {
  std::function<std::optional<std::uint64_t>(std::uint32_t)> resolve_reference;
  std::function<std::optional<std::uint64_t>(std::string_view)> resolve_name;
  std::function<void(std::uint64_t, std::uint16_t, std::uint32_t,
                     std::uint64_t)> direct_dispatch;
};

// Admits only the already-prepared first-cut command targets into the shared
// live-target boundary. It is deliberately independent of event registration
// and target behavior: callers provide the synchronous owner/component
// dispatches after recovering them. The router must outlive every command
// session built with command_session_services().
class FirstCutRuntimeCommandRouter final {
public:
  explicit FirstCutRuntimeCommandRouter(const graphics::IntroRuntime& runtime)
      : sender_(sender_from(runtime)) {
    for (const auto& target : runtime.first_cut_command_target_provenance()) {
      targets_.register_owner({.owner = target.owner.value,
                               .authored_reference = target.authored_reference,
                               .name = {}});
    }
    targets_.register_sender(sender_);
  }

  FirstCutRuntimeCommandRouter(const FirstCutRuntimeCommandRouter&) = delete;
  FirstCutRuntimeCommandRouter& operator=(const FirstCutRuntimeCommandRouter&) = delete;
  FirstCutRuntimeCommandRouter(FirstCutRuntimeCommandRouter&&) = delete;
  FirstCutRuntimeCommandRouter& operator=(FirstCutRuntimeCommandRouter&&) = delete;

  [[nodiscard]] std::uint64_t sender() const noexcept { return sender_; }
  [[nodiscard]] std::optional<std::uint64_t>
  resolve_reference(std::uint32_t reference) const noexcept {
    return targets_.resolve_reference(reference);
  }

  [[nodiscard]] FirstCutCommandSessionServices command_session_services(
      runtime::IntroLiveTargetRegistry::DispatchServices dispatch) {
    if (!dispatch.direct_target || !dispatch.direct_component) {
      throw std::runtime_error("first-cut runtime command router dispatch is incomplete");
    }
    return {
        .resolve_reference = [this](std::uint32_t reference) {
          return resolve_reference(reference);
        },
        .resolve_name = {},
        .direct_dispatch = [this, dispatch = std::move(dispatch)](
                               std::uint64_t target, std::uint16_t event,
                               std::uint32_t argument, std::uint64_t sender) {
          targets_.dispatch(target, event, argument, sender, dispatch);
        },
    };
  }

private:
  [[nodiscard]] static std::uint64_t sender_from(
      const graphics::IntroRuntime& runtime) {
    const auto* player = runtime.first_cut_player_prepared_state();
    if (player == nullptr || player->list_owner.value == 0U) {
      throw std::runtime_error("first-cut runtime command router requires prepared list owner");
    }
    return player->list_owner.value;
  }

  runtime::IntroLiveTargetRegistry targets_;
  std::uint64_t sender_{};
};

// Binds CommandPass to the checked first-cut command records and the runtime's
// prepared authored-event mapping.  This is not a player: it neither samples a
// clock nor starts a cut, frames, creates a view, submits a frame, or invokes
// SDL.  The caller supplies the already-sampled timeline position explicitly.
class FirstCutCommandSession final {
public:
  FirstCutCommandSession(const graphics::IntroRuntime& runtime, float derived_end,
                         FirstCutCommandSessionServices services,
                         std::uint64_t owner_sender)
      : FirstCutCommandSession(runtime.resources().first_cut().commands,
                               runtime.source_event_name_mapping(), derived_end,
                               std::move(services), owner_sender) {}

  // This prepared-input form exists for focused deterministic tests and for a
  // future host that retains the same checked prepared data separately from an
  // IntroRuntime.  It has the identical bounded mapping rules as the runtime
  // constructor above.
  FirstCutCommandSession(
      std::span<const data::GmsIntroCutCommandSource> commands,
      std::span<const std::optional<std::uint32_t>> event_mapping,
      float derived_end, FirstCutCommandSessionServices services,
      std::uint64_t owner_sender)
      : event_mapping_(event_mapping.begin(), event_mapping.end()),
        delivery_(delivery_services(std::move(services), event_mapping_), owner_sender),
        pass_(commands, derived_end) {}

  FirstCutCommandSession(const FirstCutCommandSession&) = delete;
  FirstCutCommandSession& operator=(const FirstCutCommandSession&) = delete;
  FirstCutCommandSession(FirstCutCommandSession&&) = delete;
  FirstCutCommandSession& operator=(FirstCutCommandSession&&) = delete;

  // Each due command is synchronously resolved and dispatched by the injected
  // owner boundary.  An unresolved event or target still returns normally to
  // CommandPass, preserving its recovered consumption behavior.
  void run(float sampled_position) {
    pass_.run(sampled_position, [this](const auto& command, std::size_t) {
      static_cast<void>(delivery_.deliver(command));
    });
  }

  void reset_start() { pass_.reset_start(); }

private:
  static CommandDeliveryServices delivery_services(
      FirstCutCommandSessionServices supplied,
      const std::vector<std::optional<std::uint32_t>>& event_mapping) {
    if (!supplied.resolve_reference || !supplied.direct_dispatch)
      throw std::runtime_error("first-cut command session services are incomplete");
    CommandDeliveryServices result;
    result.resolve_event = [&event_mapping](std::uint32_t reference)
        -> std::optional<std::uint16_t> {
      if (reference >= event_mapping.size() || !event_mapping[reference] ||
          *event_mapping[reference] == 0U ||
          *event_mapping[reference] > std::numeric_limits<std::uint16_t>::max())
        return std::nullopt;
      return static_cast<std::uint16_t>(*event_mapping[reference]);
    };
    result.resolve_reference = std::move(supplied.resolve_reference);
    result.resolve_name = supplied.resolve_name
        ? std::move(supplied.resolve_name)
        : [](std::string_view) -> std::optional<std::uint64_t> { return std::nullopt; };
    result.direct_dispatch = std::move(supplied.direct_dispatch);
    return result;
  }

  // Declare this before delivery_: its copied mapping must outlive the lambda
  // held by CommandDeliveryAdapter.
  std::vector<std::optional<std::uint32_t>> event_mapping_;
  CommandDeliveryAdapter delivery_;
  CommandPass pass_;
};

} // namespace off::cutscene
