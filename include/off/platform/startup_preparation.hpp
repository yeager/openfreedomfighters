#pragma once

#include "off/data/install.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace off::platform {

enum class StartupPreparationOutcome {
  ready,
  verification_error,
  preparation_error,
  cancelled
};
struct StartupPreparationResult {
  StartupPreparationOutcome outcome{
      StartupPreparationOutcome::preparation_error};
  data::InstallVerification verification;
  std::string message;
};

// CPU-only orchestration boundary. Production callers must supply the actual
// installation verifier; injection exists for unit tests, not ownership bypass.
// Cancellation is cooperative between stages. An active parser is never killed.
// Callback-owned output must not be read until this function has completed.
[[nodiscard]] StartupPreparationResult
prepare_startup_cpu(const std::function<data::InstallVerification()> &verify,
                    const std::function<void()> &prepare_assets,
                    const std::atomic_bool &cancelled);

} // namespace off::platform
