#include "off/settings/upscaler_runtime.hpp"

#include <array>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

off::settings::UpscalerRuntimeBinding complete(
    off::settings::Upscaler backend) {
  return {.backend = backend,
          .native_device_ready = true,
          .temporal_inputs_ready = true,
          .output_target_ready = true,
          .ui_composed_after_resolve = true,
          .history_invalidation_bound = true,
          .submit_bound = true,
          .runtime_name = "test-runtime",
          .runtime_version = "1.0"};
}
} // namespace

int main() {
  using namespace off::settings;
  GraphicsCapabilities base;
  base.temporal_upscaler = true;
  base.dlss_upscaler = true;
  base.fsr_upscaler = true;
  base.xess_upscaler = true;
  base.modern_plus = true;

  const auto unbound = negotiate_upscaler_runtime_capabilities(base, {});
  check(!unbound.temporal_upscaler && !unbound.dlss_upscaler &&
            !unbound.fsr_upscaler && !unbound.xess_upscaler,
        "configuration claims alone never expose an upscaler");

  auto temporal = complete(Upscaler::temporal);
  auto dlss = complete(Upscaler::dlss);
  auto fsr = complete(Upscaler::fsr);
  auto xess = complete(Upscaler::xess);
  const std::array ready{temporal, dlss, fsr, xess};
  const auto negotiated = negotiate_upscaler_runtime_capabilities(base, ready);
  check(negotiated.temporal_upscaler && negotiated.dlss_upscaler &&
            negotiated.fsr_upscaler && negotiated.xess_upscaler,
        "a single complete native binding enables its matching backend");

  auto incomplete = complete(Upscaler::fsr);
  incomplete.submit_bound = false;
  const std::array partly_ready{temporal, incomplete};
  const auto incomplete_result =
      negotiate_upscaler_runtime_capabilities(base, partly_ready);
  check(incomplete_result.temporal_upscaler && !incomplete_result.fsr_upscaler,
        "a runtime without a submission binding fails closed");

  const std::array duplicate{dlss, dlss};
  const auto duplicate_result =
      negotiate_upscaler_runtime_capabilities(base, duplicate);
  check(!duplicate_result.dlss_upscaler,
        "ambiguous provider bindings fail closed instead of selecting one");

  auto no_modern_plus = base;
  no_modern_plus.modern_plus = false;
  const std::array vendor_and_temporal{temporal, xess};
  const auto no_plus_result =
      negotiate_upscaler_runtime_capabilities(no_modern_plus, vendor_and_temporal);
  check(no_plus_result.temporal_upscaler && !no_plus_result.xess_upscaler,
        "vendor backends remain unavailable outside Modern+");

  auto nameless = complete(Upscaler::temporal);
  nameless.runtime_version = {};
  const std::array bad_identity{nameless};
  const auto bad_identity_result =
      negotiate_upscaler_runtime_capabilities(base, bad_identity);
  check(!bad_identity_result.temporal_upscaler,
        "an unnamed or unversioned runtime is never reported as active");

  auto missing_output = complete(Upscaler::dlss);
  missing_output.output_target_ready = false;
  const std::array incomplete_output{missing_output};
  const auto missing_output_result =
      negotiate_upscaler_runtime_capabilities(base, incomplete_output);
  check(!missing_output_result.dlss_upscaler,
        "a provider without an output target is never exposed");

  auto scales_ui = complete(Upscaler::fsr);
  scales_ui.ui_composed_after_resolve = false;
  const std::array incomplete_ui_order{scales_ui};
  const auto incomplete_ui_order_result =
      negotiate_upscaler_runtime_capabilities(base, incomplete_ui_order);
  check(!incomplete_ui_order_result.fsr_upscaler,
        "a provider that can scale UI or subtitles is never exposed");

  auto stale_history = complete(Upscaler::xess);
  stale_history.history_invalidation_bound = false;
  const std::array incomplete_history_lifecycle{stale_history};
  const auto incomplete_history_lifecycle_result =
      negotiate_upscaler_runtime_capabilities(base,
                                              incomplete_history_lifecycle);
  check(!incomplete_history_lifecycle_result.xess_upscaler,
        "a provider without cut and resize history invalidation is never exposed");
  return failures == 0 ? 0 : 1;
}
