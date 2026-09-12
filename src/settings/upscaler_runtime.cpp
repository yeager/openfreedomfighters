#include "off/settings/upscaler_runtime.hpp"

#include <array>

namespace off::settings {
namespace {

[[nodiscard]] constexpr bool provider_backend(Upscaler backend) noexcept {
  return backend == Upscaler::temporal || backend == Upscaler::dlss ||
         backend == Upscaler::fsr || backend == Upscaler::xess;
}

[[nodiscard]] constexpr std::size_t provider_index(Upscaler backend) noexcept {
  switch (backend) {
  case Upscaler::temporal:
    return 0;
  case Upscaler::dlss:
    return 1;
  case Upscaler::fsr:
    return 2;
  case Upscaler::xess:
    return 3;
  case Upscaler::native:
    break;
  }
  return 4;
}

[[nodiscard]] bool complete(const UpscalerRuntimeBinding &binding) noexcept {
  return provider_backend(binding.backend) && binding.native_device_ready &&
         binding.temporal_inputs_ready && binding.submit_bound &&
         !binding.runtime_name.empty() && !binding.runtime_version.empty();
}

} // namespace

GraphicsCapabilities negotiate_upscaler_runtime_capabilities(
    GraphicsCapabilities base,
    std::span<const UpscalerRuntimeBinding> bindings) noexcept {
  // Do not allow a configuration bit, a product name, or SDK discovery alone
  // to make a vendor backend appear active.
  base.temporal_upscaler = false;
  base.dlss_upscaler = false;
  base.fsr_upscaler = false;
  base.xess_upscaler = false;

  std::array<unsigned, 4> claims{};
  std::array<bool, 4> complete_claim{};
  for (const auto &binding : bindings) {
    if (!provider_backend(binding.backend))
      continue;
    const auto index = provider_index(binding.backend);
    ++claims[index];
    complete_claim[index] = complete(binding);
  }

  // A duplicate must not select an arbitrary SDK/runtime instance. Incomplete
  // bindings likewise remain unavailable until the renderer can submit frames.
  if (claims[0] == 1 && complete_claim[0])
    base.temporal_upscaler = true;
  if (base.modern_plus && claims[1] == 1 && complete_claim[1])
    base.dlss_upscaler = true;
  if (base.modern_plus && claims[2] == 1 && complete_claim[2])
    base.fsr_upscaler = true;
  if (base.modern_plus && claims[3] == 1 && complete_claim[3])
    base.xess_upscaler = true;
  return base;
}

} // namespace off::settings
