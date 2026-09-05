#include "off/settings/graphics_settings.hpp"

#include <utility>

namespace off::settings {
namespace {

[[nodiscard]] bool valid(Mode value) {
  return value == Mode::original || value == Mode::modern;
}

[[nodiscard]] bool valid(WindowMode value) {
  return value == WindowMode::windowed ||
         value == WindowMode::borderless_desktop;
}

[[nodiscard]] bool valid(PresentMode value) {
  return value == PresentMode::vsync || value == PresentMode::mailbox ||
         value == PresentMode::immediate;
}

[[nodiscard]] bool valid(Upscaler value) {
  return value == Upscaler::native || value == Upscaler::temporal ||
         value == Upscaler::dlss;
}

[[nodiscard]] bool valid(ShadowQuality value) {
  return value == ShadowQuality::reference || value == ShadowQuality::high ||
         value == ShadowQuality::ultra;
}

} // namespace

GraphicsResolution
resolve_graphics_settings(const RequestedGraphicsSettings &requested,
                          const GraphicsCapabilities &capabilities) {
  if (!valid(requested.profile) || !valid(requested.window_mode) ||
      !valid(requested.present_mode) || !valid(requested.upscaler) ||
      !valid(requested.shadow_quality)) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::invalid_enum};
  }
  if (requested.windowed_size.width == 0 ||
      requested.windowed_size.height == 0) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::zero_window_dimension};
  }
  if (requested.windowed_size.width <
          capabilities.minimum_windowed_size.width ||
      requested.windowed_size.height <
          capabilities.minimum_windowed_size.height) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::window_size_below_minimum};
  }
  if (requested.windowed_size.width >
          capabilities.maximum_windowed_size.width ||
      requested.windowed_size.height >
          capabilities.maximum_windowed_size.height) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::window_size_above_maximum};
  }
  if (requested.render_scale_percent < 50 ||
      requested.render_scale_percent > 200) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::render_scale_out_of_range};
  }
  if (!capabilities.original_profile && !capabilities.modern_profile) {
    return {.effective = std::nullopt,
            .error = GraphicsValidationError::no_supported_profile};
  }

  EffectiveGraphicsSettings effective{
      .profile = requested.profile,
      .window_mode = requested.window_mode,
      .windowed_size = requested.windowed_size,
      .present_mode = requested.present_mode,
      .modern_plus = requested.modern_plus,
      .render_scale_percent = requested.render_scale_percent,
      .upscaler = requested.upscaler,
      .shadow_quality = requested.shadow_quality,
      .fallbacks = {},
  };
  const auto requested_profile_available = requested.profile == Mode::original
                                               ? capabilities.original_profile
                                               : capabilities.modern_profile;
  if (!requested_profile_available) {
    effective.profile =
        capabilities.original_profile ? Mode::original : Mode::modern;
    effective.fallbacks.push_back(
        {GraphicsField::profile, FallbackReason::profile_unavailable});
  }
  if (effective.profile == Mode::original) {
    effective.modern_plus = false;
    effective.upscaler = Upscaler::native;
    effective.shadow_quality = ShadowQuality::reference;
  } else {
    if (effective.modern_plus && !capabilities.modern_plus) {
      effective.modern_plus = false;
      effective.fallbacks.push_back({GraphicsField::modern_plus,
                                     FallbackReason::modern_plus_unavailable});
    }
    if (effective.upscaler == Upscaler::dlss && !capabilities.dlss_upscaler) {
      effective.upscaler = capabilities.temporal_upscaler ? Upscaler::temporal
                                                          : Upscaler::native;
      effective.fallbacks.push_back(
          {GraphicsField::upscaler, FallbackReason::dlss_upscaler_unavailable});
    } else if (effective.upscaler == Upscaler::temporal &&
               !capabilities.temporal_upscaler) {
      effective.upscaler = Upscaler::native;
      effective.fallbacks.push_back(
          {GraphicsField::upscaler,
           FallbackReason::temporal_upscaler_unavailable});
    }
  }
  if (effective.window_mode == WindowMode::borderless_desktop &&
      !capabilities.borderless_desktop) {
    effective.window_mode = WindowMode::windowed;
    effective.fallbacks.push_back(
        {GraphicsField::window_mode, FallbackReason::borderless_unavailable});
  }
  if (effective.present_mode == PresentMode::mailbox &&
      !capabilities.mailbox_present) {
    effective.present_mode = PresentMode::vsync;
    effective.fallbacks.push_back(
        {GraphicsField::present_mode, FallbackReason::mailbox_unavailable});
  } else if (effective.present_mode == PresentMode::immediate &&
             !capabilities.immediate_present) {
    effective.present_mode = PresentMode::vsync;
    effective.fallbacks.push_back(
        {GraphicsField::present_mode, FallbackReason::immediate_unavailable});
  }
  return {.effective = std::move(effective), .error = std::nullopt};
}

bool requires_display_confirmation(
    const EffectiveGraphicsSettings &before,
    const EffectiveGraphicsSettings &after) noexcept {
  return before.window_mode != after.window_mode ||
         before.windowed_size != after.windowed_size ||
         before.present_mode != after.present_mode;
}

} // namespace off::settings
