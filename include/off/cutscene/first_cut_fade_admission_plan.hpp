#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace off::cutscene {

// This is a checked hand-off between recovered loader/lifecycle evidence and a
// future live-target registration.  It deliberately has no catalogue, runtime,
// registry, renderer, or startup dependency.  Supplying the evidence remains
// the caller's responsibility; this type only rejects an incomplete hand-off.
struct FirstCutFadeAttachmentEvidence final {
    std::uint16_t source_row{};
    std::uint32_t authored_owner_reference{};
    std::size_t attachment_count{};
    std::string_view attachment_identifier{};
    std::uint32_t attachment_argument{};
    std::uint64_t canonical_owner_handle{};
    std::uint64_t canonical_component_handle{};
    bool owner_hidden{};
    std::uint16_t fade_in_event{};
    std::uint16_t fade_out_event{};
};

struct FirstCutFadeAdmissionEvidence final {
    bool directory_construction_complete{};
    bool reader_bracket_complete{};
    bool lifecycle_phase_one_complete{};
    bool lifecycle_phase_two_complete{};
    std::array<FirstCutFadeAttachmentEvidence, 3> attachments{};
};

// A future caller may translate these data-only records to its live registry.
// No registration, dispatch, owner control, or fade construction happens here.
struct FirstCutFadeRegistrationSpec final {
    std::uint16_t source_row{};
    std::uint32_t authored_owner_reference{};
    std::uint64_t canonical_owner_handle{};
    std::uint64_t canonical_component_handle{};
    std::uint16_t fade_in_event{};
    std::uint16_t fade_out_event{};
};

class FirstCutFadeAdmissionPlan final {
public:
    FirstCutFadeAdmissionPlan(const FirstCutFadeAdmissionPlan&) = delete;
    FirstCutFadeAdmissionPlan& operator=(const FirstCutFadeAdmissionPlan&) = delete;
    FirstCutFadeAdmissionPlan(FirstCutFadeAdmissionPlan&&) = delete;
    FirstCutFadeAdmissionPlan& operator=(FirstCutFadeAdmissionPlan&&) = delete;

    [[nodiscard]] const std::array<FirstCutFadeRegistrationSpec, 3>&
    registration_specs() const noexcept {
        return registration_specs_;
    }

private:
    friend FirstCutFadeAdmissionPlan make_first_cut_fade_admission_plan(
        const FirstCutFadeAdmissionEvidence& evidence);

    explicit FirstCutFadeAdmissionPlan(std::array<FirstCutFadeRegistrationSpec, 3> specs)
        : registration_specs_(specs) {}

    std::array<FirstCutFadeRegistrationSpec, 3> registration_specs_;
};

[[nodiscard]] inline FirstCutFadeAdmissionPlan make_first_cut_fade_admission_plan(
    const FirstCutFadeAdmissionEvidence& evidence) {
    if (!evidence.directory_construction_complete || !evidence.reader_bracket_complete ||
        !evidence.lifecycle_phase_one_complete || !evidence.lifecycle_phase_two_complete) {
        throw std::runtime_error("first cut fade admission requires completed loader and lifecycle boundaries");
    }

    constexpr std::array<std::uint16_t, 3> expected_rows{4, 7, 9};
    constexpr std::array<std::uint32_t, 3> expected_references{
        0x80000005U, 0x80000008U, 0x8000000aU};
    std::array<FirstCutFadeRegistrationSpec, 3> specs{};
    std::uint16_t retained_fade_in{};
    std::uint16_t retained_fade_out{};
    for (std::size_t index = 0; index != evidence.attachments.size(); ++index) {
        const auto& item = evidence.attachments[index];
        if (item.source_row != expected_rows[index] ||
            item.authored_owner_reference != expected_references[index] ||
            item.attachment_count != 1 || item.attachment_identifier != "ZWINPIC_FadeToBlack" ||
            item.attachment_argument != 0 || item.canonical_owner_handle == 0 ||
            item.canonical_component_handle == 0 || item.owner_hidden || item.fade_in_event == 0 ||
            item.fade_out_event == 0 || item.fade_in_event == item.fade_out_event) {
            throw std::runtime_error("first cut fade admission has unsupported source evidence");
        }
        if (index == 0) {
            retained_fade_in = item.fade_in_event;
            retained_fade_out = item.fade_out_event;
        } else if (item.fade_in_event != retained_fade_in || item.fade_out_event != retained_fade_out) {
            throw std::runtime_error("first cut fade admission requires shared concrete event identifiers");
        }
        for (std::size_t earlier = 0; earlier != index; ++earlier) {
            const auto& previous = evidence.attachments[earlier];
            if (item.authored_owner_reference == previous.authored_owner_reference ||
                item.canonical_owner_handle == previous.canonical_owner_handle ||
                item.canonical_component_handle == previous.canonical_component_handle) {
                throw std::runtime_error("first cut fade admission requires distinct canonical targets");
            }
        }
        specs[index] = {item.source_row, item.authored_owner_reference, item.canonical_owner_handle,
                        item.canonical_component_handle, item.fade_in_event, item.fade_out_event};
    }
    return FirstCutFadeAdmissionPlan{specs};
}

} // namespace off::cutscene
