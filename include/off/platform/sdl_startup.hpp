#pragma once

#include "off/data/install.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace off::platform {

enum class StartupPreflightOutcome {
  ready,
  data_error,
  quit_requested,
  platform_error,
};

struct StartupPreflightResult {
  StartupPreflightOutcome outcome{StartupPreflightOutcome::platform_error};
  data::InstallVerification verification;
  std::string message;
};

// Opens the project-owned splash before touching retail data. This entry point
// is intentionally not used by --verify-only, --help, or --version.
// prepare_assets runs on the verification worker after successful verification.
// It must perform CPU-only work; captures remain alive until this call returns.
// The worker is always joined before return, including cancellation and errors.
[[nodiscard]] StartupPreflightResult
run_sdl_startup_preflight(const std::filesystem::path &data_path,
                          const std::function<void()> &prepare_assets);

} // namespace off::platform
