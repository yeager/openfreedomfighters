#include "off/graphics/fresh_intro_camera.hpp"
#include <bit>
#include <stdexcept>

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

FreshIntroCamera::FreshIntroCamera()
    : parameters_{std::nullopt,5.0F,20000.0F,400.0F,std::bit_cast<float>(0x3f9c61abU),
                  {0.0F,0.0F,1.0F,1.0F},1.0F,0.0F,0U,false,0.0F,0.0F},
      enabled_state_(0x20U) {}

FreshIntroCamera::FreshIntroCamera(const data::GmsIntroCameraSource &source)
    : parameters_(convert_intro_camera_mode_zero(source)),
      enabled_state_(fresh_source_flags(source)),
      priority_(std::bit_cast<std::int32_t>(source.priority)) {}

void FreshIntroCamera::set_priority(std::int32_t priority) noexcept {
  const volatile float rounded=static_cast<float>(priority);
  priority_=priority;
  parameters_.registration_priority=rounded;
}

void FreshIntroCamera::notify_renderer_dimensions(
    const std::function<std::int32_t()>& width,
    const std::function<std::int32_t()>& height) {
  if(notifying_dimensions_) throw std::runtime_error("camera dimension notification cannot reenter");
  struct Guard {
    bool& active;
    explicit Guard(bool& value):active(value) {active=true;}
    ~Guard() {active=false;}
  } guard(notifying_dimensions_);
  if(!width) throw std::runtime_error("camera dimension notification requires width service");
  const volatile float converted_width=static_cast<float>(width());
  renderer_width_=converted_width;
  if(!height) throw std::runtime_error("camera dimension notification requires height service");
  const volatile float converted_height=static_cast<float>(height());
  renderer_height_=converted_height;
}

void FreshIntroCamera::set_enabled(bool requested, bool renderer_present,
                                   const std::function<void()> &state_change) {
  enabled_state_.set_enabled(requested, renderer_present, state_change);
}

void FreshIntroCamera::prepare_picture_services(const PictureVisitorRectangle& rectangle) {
  picture_services_ = prepare_picture_camera_services(enabled_state_, parameters_, rectangle);
}

void FreshIntroCamera::apply_window_state_projection(bool option_a, bool option_b,
                                                    std::uint64_t window_handle) {
  if (window_handle == 0 || enabled_state_.changing_)
    throw std::runtime_error("window camera projection requires a live nonreentrant target");
  render_control_ = option_b ? 0U : 5U;
  if (option_a && !option_b) enabled_state_.flags_ |= 0x8000U;
  else enabled_state_.flags_ &= ~0x8000U;
  associated_target_ = window_handle;
  enabled_state_.flags_ |= 0x210000U;
}

} // namespace off::graphics
