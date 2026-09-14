#include "off/platform/first_cut_picture_frame.hpp"
#include "off/platform/sdl_first_cut_picture_frame_bridge.hpp"
#include <cstdlib>
#include <iostream>
#include <type_traits>

namespace {
void check(bool ok) { if (!ok) std::exit(1); }
}

int main() {
  using namespace off;
  platform::FirstCutPictureFrame frame;
  static_assert(!std::is_default_constructible_v<platform::FirstCutFrameAdmissionPermit>);
  static_assert(!std::is_copy_constructible_v<platform::FirstCutFrameAdmissionPermit>);
  static_assert(!std::is_copy_assignable_v<platform::FirstCutFrameAdmissionPermit>);
  static_assert(std::is_move_constructible_v<platform::FirstCutFrameAdmissionPermit>);

  // A standalone frame has no authority to invent a view admission permit.
  // NormalIntroSceneHost owns the only minting point; its focused tests cover
  // the successful and consumed-permit paths.
  check(frame.submissions().empty());
  check(!frame.ready_for_render());
  bool rejected{};
  try { (void)platform::SdlFirstCutPictureFrameBridge::bind(frame); } catch (...) { rejected = true; }
  check(rejected);
}
