#pragma once

#include "off/graphics/render_scale.hpp"
#include "off/settings/graphics_settings.hpp"

#include <optional>

namespace off::platform {

// The SDL GPU renderer consumes this compact, already-resolved state for every
// acquired swapchain image.  It deliberately accepts only native presentation:
// temporal and vendor entries are user intent/capability negotiation states,
// not implemented SDL GPU presentation backends yet.
struct RuntimePresentationSettings {
  Mode profile{Mode::original};
  graphics::RenderScaleExtent output_extent{};
  graphics::RenderScaleExtent content_extent{};
  bool spatial_resample{false};
  friend bool operator==(const RuntimePresentationSettings &,
                         const RuntimePresentationSettings &) = default;
};

// Modern+ is an implementation capability, not a profile label.  A platform
// renderer must clear it until it has bound the enhanced presentation path it
// intends to use.  Vendor provider bits are Modern+-only as well, so clearing
// the profile capability keeps stale discovery/configuration claims out of F10
// before settings resolution.
[[nodiscard]] settings::GraphicsCapabilities
negotiate_runtime_presentation_capabilities(
    settings::GraphicsCapabilities base,
    bool modern_plus_presentation_ready) noexcept;

// Fails closed if an invalid extent, scale, or unbound upscaler reaches the
// renderer.  This makes the actual frame plan derive from the F10 effective
// state rather than a command-line profile or an unverified backend claim.
[[nodiscard]] std::optional<RuntimePresentationSettings>
resolve_runtime_presentation_settings(
    const settings::EffectiveGraphicsSettings &effective,
    graphics::RenderScaleExtent output_extent) noexcept;

} // namespace off::platform
