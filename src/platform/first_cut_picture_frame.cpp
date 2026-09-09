#include "off/platform/first_cut_picture_frame.hpp"

namespace off::platform {

FirstCutPictureFrameResult FirstCutPictureFrame::assemble(
    const FirstCutPictureFrameInput &input) {
  submissions_.clear();
  ready_for_render_ = false;
  ++assembly_generation_;
  if (input.view_admission != graphics::FirstCutViewAdmissionResult::view_admitted)
    return FirstCutPictureFrameResult::view_not_admitted;
  if (!input.positive_time_member_activated)
    return FirstCutPictureFrameResult::member_not_activated;
  if (!input.activation_prefix_complete)
    return FirstCutPictureFrameResult::activation_incomplete;
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
