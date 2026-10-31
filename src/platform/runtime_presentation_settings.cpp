#include "off/platform/runtime_presentation_settings.hpp"

namespace off::platform {

settings::GraphicsCapabilities negotiate_runtime_presentation_capabilities(
    settings::GraphicsCapabilities base,
    bool modern_plus_presentation_ready) noexcept {
  if (modern_plus_presentation_ready)
    return base;
  base.modern_plus = false;
  base.dlss_upscaler = false;
  base.fsr_upscaler = false;
  base.xess_upscaler = false;
  return base;
}

std::optional<RuntimePresentationSettings> resolve_runtime_presentation_settings(
    const settings::EffectiveGraphicsSettings &effective,
    graphics::RenderScaleExtent output_extent,
    bool fsr1_spatial_available) noexcept {
  // The renderer's capabilities are negotiated before F10 can apply a value.
  // Keep a second boundary at consumption time: no requested or stale backend
  // identifier may be treated as an implemented presentation algorithm.
  const bool fsr1_selected =
      effective.upscaler == settings::Upscaler::fsr1_spatial;
  if (effective.upscaler != settings::Upscaler::native && !fsr1_selected)
    return std::nullopt;
  const auto content_extent = graphics::resolve_render_scale_extent(
      output_extent, effective.render_scale_percent);
  if (!content_extent)
    return std::nullopt;
  const bool spatial_resample = *content_extent != output_extent;
  const bool fsr1_active =
      fsr1_selected && fsr1_spatial_available &&
      content_extent->width < output_extent.width &&
      content_extent->height < output_extent.height;
  const bool fsr1_upscale_requested =
      content_extent->width < output_extent.width &&
      content_extent->height < output_extent.height;
  return RuntimePresentationSettings{
      .profile = effective.profile,
      .output_extent = output_extent,
      .content_extent = *content_extent,
      .spatial_resample = spatial_resample,
      .fsr1_spatial_active = fsr1_active,
      .fsr1_spatial_fallback = fsr1_selected && fsr1_upscale_requested &&
                               !fsr1_active,
  };
}

} // namespace off::platform
