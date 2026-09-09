#pragma once

#include "off/graphics/first_cut_view_admission_gate.hpp"
#include "off/platform/intro_picture_submission.hpp"

#include <cstdint>
#include <optional>

namespace off::platform {

// Evidence produced by the caller's actual lifecycle and ordered traversal.
// It deliberately contains no fallback route or inferred admission.
struct FirstCutPictureFrameInput final {
  graphics::FirstCutViewAdmissionResult view_admission{};
  bool positive_time_member_activated{};
  bool activation_prefix_complete{};
  bool ordered_record_accepted{};
  std::span<const IntroPictureSubmissionInput> pictures;
};

enum class FirstCutPictureFrameResult : std::uint8_t {
  view_not_admitted, member_not_activated, activation_incomplete,
  ordered_record_not_accepted, no_pictures, assembled,
};

class FirstCutPictureFrame final {
public:
  [[nodiscard]] FirstCutPictureFrameResult assemble(
      const FirstCutPictureFrameInput &input);
  [[nodiscard]] std::span<const IntroPictureSubmission> submissions() const noexcept {
    return submissions_;
  }
  // True only after this instance accepted every explicit lifecycle gate and
  // assembled at least one submitted picture.  An unsuccessful later attempt
  // clears this state with its submissions.
  [[nodiscard]] bool ready_for_render() const noexcept { return ready_for_render_; }
  // Changes on every assembly attempt, including a rejected one.  Consumers
  // retaining submission spans can use this to reject stale frame contents.
  [[nodiscard]] std::uint64_t assembly_generation() const noexcept {
    return assembly_generation_;
  }

private:
  std::vector<IntroPictureSubmission> submissions_;
  bool ready_for_render_{};
  std::uint64_t assembly_generation_{};
};

} // namespace off::platform
