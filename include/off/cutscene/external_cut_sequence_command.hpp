#pragma once

#include "off/data/gms_image.hpp"

#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace off::cutscene {

// Common command data and the externally hosted list reference are separate
// authored fields.  Neither is inferred from a source row or attachment type.
struct ExternalCutSequenceCommandReader final {
    std::function<data::GmsIntroCutCommandSource()> read_common_command;
    std::function<std::uint32_t()> read_external_list_reference;
};

// Already-admitted phase-two boundaries.  They operate solely on live scene
// objects and deliberately own no lifecycle traversal, startup, clock, SDL,
// renderer, or event-dispatch policy.
struct ExternalCutSequenceCommandPhaseTwoServices final {
    std::function<std::optional<std::uint64_t>(std::uint32_t)> resolve_reference;
    std::function<std::optional<std::uint64_t>(std::uint64_t, std::string_view)>
        find_owner_component;
    // The target list owns ordering and retention. Called only for a
    // nonnegative signed timeline word.
    std::function<void(std::uint64_t, const data::GmsIntroCutCommandSource&)>
        register_ordered_command;
    // Missing property becomes the recovered zero-handle fallback, which is
    // still supplied to resolve_scene_object.
    std::function<std::optional<std::uint64_t>(std::string_view)> read_scene_handle;
    std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_scene_object;
    std::function<void()> retire_diagnostic;
};

// Bounded ZLIST_ExternCutSequenceCommand model. It is not a scheduler or
// player: phase two cannot deliver authored events or start a cut.
class ExternalCutSequenceCommand final {
public:
    ExternalCutSequenceCommand() = default;

    void read(const ExternalCutSequenceCommandReader& reader) {
        if (read_ || retired_ || phase_two_completed_ || !reader.read_common_command ||
            !reader.read_external_list_reference)
            throw std::runtime_error("external cut command reader boundary is invalid");
        command_ = reader.read_common_command();
        external_list_reference_ = reader.read_external_list_reference();
        read_ = true;
    }

    // Resolve external owner, then its exact concrete CutSequenceList
    // component. An unresolved owner/list retires before property lookup.
    void run_phase_two(const ExternalCutSequenceCommandPhaseTwoServices& services) {
        if (!read_ || retired_ || phase_two_completed_ || !services.resolve_reference ||
            !services.find_owner_component || !services.register_ordered_command ||
            !services.read_scene_handle || !services.resolve_scene_object ||
            !services.retire_diagnostic)
            throw std::runtime_error("external cut command phase-two boundary is invalid");
        const auto owner = services.resolve_reference(external_list_reference_);
        if (!owner) return retire(services);
        const auto list = services.find_owner_component(*owner, "CutSequenceList");
        if (!list) return retire(services);
        if (std::bit_cast<std::int32_t>(command_.timeline_position) >= 0)
            services.register_ordered_command(*list, command_);
        const auto handle = services.read_scene_handle("rCutSequenceObjects").value_or(0U);
        cached_context_ = services.resolve_scene_object(handle);
        phase_two_completed_ = true;
    }

    [[nodiscard]] bool read_complete() const noexcept { return read_; }
    [[nodiscard]] bool retired() const noexcept { return retired_; }
    [[nodiscard]] bool phase_two_completed() const noexcept { return phase_two_completed_; }
    [[nodiscard]] std::uint32_t external_list_reference() const noexcept { return external_list_reference_; }
    [[nodiscard]] const data::GmsIntroCutCommandSource& command() const {
        if (!read_) throw std::runtime_error("external cut command has not been read");
        return command_;
    }
    [[nodiscard]] std::optional<std::uint64_t> cached_context() const noexcept { return cached_context_; }

private:
    void retire(const ExternalCutSequenceCommandPhaseTwoServices& services) {
        services.retire_diagnostic();
        retired_ = true;
    }
    data::GmsIntroCutCommandSource command_{};
    std::uint32_t external_list_reference_{};
    std::optional<std::uint64_t> cached_context_;
    bool read_{};
    bool retired_{};
    bool phase_two_completed_{};
};

} // namespace off::cutscene
