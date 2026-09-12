#pragma once

#include "off/settings/graphics_settings.hpp"

#include <span>
#include <string_view>

namespace off::settings {

// A provider is eligible only after the active renderer has bound its native
// submission path. This is deliberately not a dynamic-library loader: vendor
// SDK discovery, licensing, and device creation stay in the platform backend.
// The renderer submits a completed binding here only after it can provide the
// required temporal inputs and execute the provider for a real frame.
struct UpscalerRuntimeBinding {
  Upscaler backend{Upscaler::native};
  bool native_device_ready{false};
  bool temporal_inputs_ready{false};
  bool submit_bound{false};
  std::string_view runtime_name;
  std::string_view runtime_version;
};

// Derives the effective provider capabilities from actual renderer bindings.
// Existing capability bits for temporal and vendor providers are treated as
// requests, never proof: only an unambiguous, complete binding can enable one.
// Duplicate or malformed claims fail closed for that provider.
[[nodiscard]] GraphicsCapabilities negotiate_upscaler_runtime_capabilities(
    GraphicsCapabilities base,
    std::span<const UpscalerRuntimeBinding> bindings) noexcept;

} // namespace off::settings
