#include "off/platform/runtime_presentation_settings.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  using off::Mode;
  using off::graphics::RenderScaleExtent;
  using off::platform::RuntimePresentationSettings;
  using off::platform::resolve_runtime_presentation_settings;
  using off::settings::EffectiveGraphicsSettings;
  using off::settings::Upscaler;

  EffectiveGraphicsSettings modern{};
  modern.profile = Mode::modern;
  modern.render_scale_percent = 67U;
  const auto plan = resolve_runtime_presentation_settings(
      modern, RenderScaleExtent{1920U, 1080U});
  check(plan == RuntimePresentationSettings{.profile = Mode::modern,
                                             .output_extent = {1920U, 1080U},
                                             .content_extent = {1287U, 724U},
                                             .spatial_resample = true},
        "the frame plan carries the effective F10 profile and render scale");

  modern.render_scale_percent = 100U;
  const auto native = resolve_runtime_presentation_settings(
      modern, RenderScaleExtent{1920U, 1080U});
  check(native && !native->spatial_resample,
        "native scale does not allocate a spatial resample path");

  modern.upscaler = Upscaler::temporal;
  check(!resolve_runtime_presentation_settings(
            modern, RenderScaleExtent{1920U, 1080U}),
        "an unbound temporal request cannot enter the SDL GPU frame path");
  modern.upscaler = Upscaler::fsr;
  check(!resolve_runtime_presentation_settings(
            modern, RenderScaleExtent{1920U, 1080U}),
        "a vendor request cannot enter the SDL GPU frame path without a backend");
  modern.upscaler = Upscaler::native;
  modern.render_scale_percent = 49U;
  check(!resolve_runtime_presentation_settings(
            modern, RenderScaleExtent{1920U, 1080U}),
        "an invalid render scale never reaches target allocation");
  return failures == 0 ? 0 : 1;
}
