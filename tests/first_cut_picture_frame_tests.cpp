#include "off/platform/first_cut_picture_frame.hpp"
#include "off/platform/sdl_first_cut_picture_frame_bridge.hpp"
#include <cstdlib>
#include <iostream>
namespace { void check(bool ok) { if (!ok) std::exit(1); } }
int main() {
  using namespace off;
  platform::FirstCutPictureFrame frame;
  platform::FirstCutPictureFrameInput input{};
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::view_not_admitted && frame.submissions().empty());
  check(!frame.ready_for_render());
  bool rejected{};
  try { (void)platform::SdlFirstCutPictureFrameBridge::bind(frame); } catch (...) { rejected = true; }
  check(rejected);
  input.view_admission = graphics::FirstCutViewAdmissionResult::view_admitted;
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::member_not_activated);
  input.positive_time_member_activated = true;
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::activation_incomplete);
  input.activation_prefix_complete = true;
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::ordered_record_not_accepted);
  input.ordered_record_accepted = true;
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::no_pictures);
  data::PictureQuad quad{}; quad.horizontal_edge_span = quad.vertical_edge_span = 1; quad.modulation_color = 0xffffffffU;
  data::BoundPictureDrawGroup group{{.image_index = 4}, {quad}};
  platform::IntroPictureSubmissionInput picture{.groups = std::span(&group, 1), .transform = {.basis = {0,0,1,0,1,0,1,0,0}}};
  input.pictures = std::span(&picture, 1);
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::assembled && frame.submissions().size() == 1 && frame.submissions()[0].draws()[0].catalog_image_index == 4);
  check(frame.ready_for_render());
  auto bridge = platform::SdlFirstCutPictureFrameBridge::bind(frame);
  check(bridge.draws().size() == 1 && bridge.draws()[0].catalog_image_index == 4);
  input.view_admission = graphics::FirstCutViewAdmissionResult::backend_absent;
  check(frame.assemble(input) == platform::FirstCutPictureFrameResult::view_not_admitted);
  check(!frame.ready_for_render());
  rejected = false;
  try { (void)bridge.draws(); } catch (...) { rejected = true; }
  check(rejected);
}
