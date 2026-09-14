#pragma once

#include "off/graphics/first_cut_view_admission_gate.hpp"
#include "off/graphics/first_cut_legal_picture_activation_receipt.hpp"
#include "off/platform/intro_picture_submission.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {
class NormalIntroSceneHost;
}

namespace off::platform {

// A move-only, one-shot authority emitted only by NormalIntroSceneHost after
// its selected first-cut view has been admitted.  It deliberately carries no
// camera identity, route, or caller-controlled "admitted" flag.
class FirstCutFrameAdmissionPermit final {
 public:
  FirstCutFrameAdmissionPermit(const FirstCutFrameAdmissionPermit&) = delete;
  FirstCutFrameAdmissionPermit& operator=(const FirstCutFrameAdmissionPermit&) = delete;
  FirstCutFrameAdmissionPermit(FirstCutFrameAdmissionPermit&& other) noexcept;
  FirstCutFrameAdmissionPermit& operator=(FirstCutFrameAdmissionPermit&& other) noexcept;

 private:
  friend class off::graphics::NormalIntroSceneHost;
  friend class FirstCutPictureFrame;

  FirstCutFrameAdmissionPermit() noexcept = default;
  [[nodiscard]] static FirstCutFrameAdmissionPermit mint() noexcept {
    return FirstCutFrameAdmissionPermit{};
  }
  [[nodiscard]] bool consume() noexcept;

  bool valid_{true};
};

// Ordered-record acceptance remains a factual producer result. Legal-picture
// activation and view admission are represented by separate non-forgeable,
// one-shot receipts consumed by FirstCutPictureFrame::assemble.
struct FirstCutPictureFrameInput final {
  bool ordered_record_accepted{};
  std::span<const IntroPictureSubmissionInput> pictures;
};

enum class FirstCutPictureFrameResult : std::uint8_t {
  admission_permit_invalid, activation_receipt_invalid, ordered_record_not_accepted,
  no_pictures, assembled,
};

class FirstCutPictureFrame final {
public:
  [[nodiscard]] FirstCutPictureFrameResult assemble(
      FirstCutFrameAdmissionPermit&& permit,
      graphics::FirstCutLegalPictureActivationReceipt&& activation,
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
