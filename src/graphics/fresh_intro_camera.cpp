#include "off/graphics/fresh_intro_camera.hpp"

namespace off::graphics {
namespace {

std::uint32_t fresh_source_flags(const data::GmsIntroCameraSource &source) {
  // The concrete constructor replaces the entire word before setting enabled.
  // These source operations are ORs, not assignments to the other flags.
  std::uint32_t flags = 0x20U;
  if (source.flag_option_a == 0U)
    flags |= 0x80000U;
  if (source.flag_option_b != 0U)
    flags |= 0x10000U;
  return flags;
}

} // namespace

FreshIntroCamera::FreshIntroCamera(const data::GmsIntroCameraSource &source)
    : parameters_(convert_intro_camera_mode_zero(source)),
      enabled_state_(fresh_source_flags(source)) {}

void FreshIntroCamera::set_enabled(bool requested, bool renderer_present,
                                   const std::function<void()> &state_change) {
  enabled_state_.set_enabled(requested, renderer_present, state_change);
}

} // namespace off::graphics
