#pragma once

#include "off/cutscene/cut_sequence_list.hpp"
#include "off/cutscene/external_cut_sequence_command.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace off::cutscene {

// The host-owned boundaries required by the recovered source-466 -> source-65
// phase-two route. They deliberately do not enumerate the scene or advance a
// lifecycle pass: the caller supplies the already-live reference/component
// services and the already phase-one-complete list instance.
struct ExternalCutSequenceListJoinServices final {
    std::function<std::optional<std::uint64_t>(std::uint32_t)> resolve_reference;
    std::function<std::optional<std::uint64_t>(std::uint64_t, std::string_view)>
        find_owner_component;
    std::function<std::optional<std::uint64_t>(std::string_view)> read_scene_handle;
    std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_scene_object;
    std::function<void()> retire_diagnostic;
};

// Binds one validated live CutSequenceList component to its concrete retained
// container. A command can reach the container only when ordinary reference and
// exact-name lookup return this component handle. This is intentionally not a
// general component registry or global lifecycle runner.
class ExternalCutSequenceListJoin final {
public:
    ExternalCutSequenceListJoin(std::uint64_t component_handle, CutSequenceList& list,
                                ExternalCutSequenceListJoinServices services)
        : component_handle_(component_handle), list_(list), services_(std::move(services)) {
        if (component_handle_ == 0U || !list_.phase_one_complete() || list_.phase_two_complete() ||
            !services_.resolve_reference || !services_.find_owner_component ||
            !services_.read_scene_handle || !services_.resolve_scene_object ||
            !services_.retire_diagnostic) {
            throw std::runtime_error("external cut command list join boundary is invalid");
        }
    }

    // Preserves the original command callback ordering: owner/component lookup,
    // cached-list insertion, then rCutSequenceObjects lookup and context cache.
    // A mismatched component is treated exactly as a missing list and retires
    // before the command can query the scene property store.
    void run_phase_two(ExternalCutSequenceCommand& command) {
        if (list_.phase_two_complete())
            throw std::runtime_error("external cut command list join is closed");
        command.run_phase_two({
            .resolve_reference = services_.resolve_reference,
            .find_owner_component = [this](std::uint64_t owner, std::string_view name) {
                const auto found = services_.find_owner_component(owner, name);
                return found && *found == component_handle_ ? found : std::optional<std::uint64_t>{};
            },
            .register_ordered_command = [this](std::uint64_t handle,
                                                const data::GmsIntroCutCommandSource& value) {
                if (handle != component_handle_)
                    throw std::runtime_error("external cut command list handle changed");
                list_.register_ordered_command(value);
            },
            .read_scene_handle = services_.read_scene_handle,
            .resolve_scene_object = services_.resolve_scene_object,
            .retire_diagnostic = services_.retire_diagnostic,
        });
    }

private:
    std::uint64_t component_handle_{};
    CutSequenceList& list_;
    ExternalCutSequenceListJoinServices services_;
};

} // namespace off::cutscene
