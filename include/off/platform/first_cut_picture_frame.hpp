#pragma once

#include "off/graphics/first_cut_view_admission_gate.hpp"
#include "off/platform/intro_picture_submission.hpp"

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

private:
  std::vector<IntroPictureSubmission> submissions_;
};

} // namespace off::platform
