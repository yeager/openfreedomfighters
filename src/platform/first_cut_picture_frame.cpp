#include "off/platform/first_cut_picture_frame.hpp"

#include <utility>

namespace off::platform {

FirstCutFrameAdmissionPermit::FirstCutFrameAdmissionPermit(
    FirstCutFrameAdmissionPermit&& other) noexcept
    : valid_(std::exchange(other.valid_, false)) {}

FirstCutFrameAdmissionPermit& FirstCutFrameAdmissionPermit::operator=(
    FirstCutFrameAdmissionPermit&& other) noexcept {
  if (this != &other)
    valid_ = std::exchange(other.valid_, false);
  return *this;
}

bool FirstCutFrameAdmissionPermit::consume() noexcept {
  return std::exchange(valid_, false);
}

FirstCutPictureFrameResult FirstCutPictureFrame::assemble(
    FirstCutFrameAdmissionPermit&& permit,
    graphics::FirstCutLegalPictureActivationReceipt&& activation,
    const FirstCutPictureFrameInput &input) {
  submissions_.clear();
  ready_for_render_ = false;
  ++assembly_generation_;
  if (!permit.consume())
    return FirstCutPictureFrameResult::admission_permit_invalid;
  if (!activation.consume())
    return FirstCutPictureFrameResult::activation_receipt_invalid;
  if (!input.ordered_record_accepted)
    return FirstCutPictureFrameResult::ordered_record_not_accepted;
  if (input.pictures.empty())
    return FirstCutPictureFrameResult::no_pictures;
  submissions_.reserve(input.pictures.size());
  for (const auto &picture : input.pictures)
    submissions_.push_back(IntroPictureSubmission::assemble(picture));
  ready_for_render_ = true;
  return FirstCutPictureFrameResult::assembled;
}

} // namespace off::platform
