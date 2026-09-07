#pragma once

#include "off/data/gms_image.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::cutscene {

// Source 65 is the already-constructed CutSequenceList addressed by the
// external fade commands. Its deferred reader grammar has not been recovered;
// this reader boundary accepts only a host-supplied, validated reader action.
struct CutSequenceListReader final {
    std::size_t source_directory{};
    std::uint32_t source_type{};
    std::uint32_t class_data_value{};
    std::function<void()> read_retained_source;
};

struct CutSequenceListPhaseServices final {
    std::function<void()> phase_complete;
};

// Bounded source-65 ZLIST_CutSequenceList model. It owns the retained ordered
// command container used by external commands, but not global lifecycle,
// playback, clock, renderer, or startup policy.
class CutSequenceList final {
public:
    static constexpr std::size_t external_fades_list_source = 65U;
    static constexpr std::uint32_t list_source_type = 0x0800001aU;

    void run_phase_one(const CutSequenceListReader& reader) {
        if (phase_one_complete_ || phase_two_complete_ || retired_ ||
            reader.source_directory != external_fades_list_source ||
            reader.source_type != list_source_type || reader.class_data_value != 0U ||
            !reader.read_retained_source) {
            throw std::runtime_error("cut sequence list phase-one boundary is invalid");
        }
        reader.read_retained_source();
        phase_one_complete_ = true;
    }

    // External commands may enter only after phase one and before this list's
    // later phase-two callback. Equal keys are inserted before the cached node;
    // this intentionally is not a stable/source-order sort.
    void register_ordered_command(const data::GmsIntroCutCommandSource& command) {
        if (!phase_one_complete_ || phase_two_complete_ || retired_ ||
            std::bit_cast<std::int32_t>(command.timeline_position) < 0) {
            throw std::runtime_error("cut sequence list command admission is invalid");
        }
        const auto key = static_cast<float>(std::bit_cast<std::int32_t>(command.timeline_position));
        std::size_t insertion{};
        if (cached_command_) {
            insertion = *cached_command_;
            while (key_at(insertion) > key) {
                if (insertion == 0U) break;
                --insertion;
            }
            while (insertion < commands_.size() && key > key_at(insertion)) ++insertion;
        }
        commands_.insert(commands_.begin() + static_cast<std::ptrdiff_t>(insertion), command);
        cached_command_ = insertion;
    }

    void run_phase_two(const CutSequenceListPhaseServices& services) {
        if (!phase_one_complete_ || phase_two_complete_ || retired_ || !services.phase_complete) {
            throw std::runtime_error("cut sequence list phase-two boundary is invalid");
        }
        services.phase_complete();
        phase_two_complete_ = true;
    }

    [[nodiscard]] bool phase_one_complete() const noexcept { return phase_one_complete_; }
    [[nodiscard]] bool phase_two_complete() const noexcept { return phase_two_complete_; }
    [[nodiscard]] const std::vector<data::GmsIntroCutCommandSource>& commands() const noexcept { return commands_; }

private:
    [[nodiscard]] float key_at(std::size_t index) const noexcept {
        return static_cast<float>(std::bit_cast<std::int32_t>(commands_[index].timeline_position));
    }
    std::vector<data::GmsIntroCutCommandSource> commands_;
    std::optional<std::size_t> cached_command_;
    bool phase_one_complete_{};
    bool phase_two_complete_{};
    bool retired_{};
};

} // namespace off::cutscene
