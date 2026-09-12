#include "off/platform/runtime_presentation_settings.hpp"

namespace off::platform {

std::optional<RuntimePresentationSettings> resolve_runtime_presentation_settings(
    const settings::EffectiveGraphicsSettings &effective,
    graphics::RenderScaleExtent output_extent) noexcept {
  // The renderer's capabilities are negotiated before F10 can apply a value.
  // Keep a second boundary at consumption time: no requested or stale backend
  // identifier may be treated as an implemented presentation algorithm.
  if (effective.upscaler != settings::Upscaler::native)
    return std::nullopt;
  const auto content_extent = graphics::resolve_render_scale_extent(
      output_extent, effective.render_scale_percent);
  if (!content_extent)
    return std::nullopt;
  return RuntimePresentationSettings{
      .profile = effective.profile,
      .output_extent = output_extent,
      .content_extent = *content_extent,
      .spatial_resample = *content_extent != output_extent,
  };
}

} // namespace off::platform
