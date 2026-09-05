#pragma once

#include "off/data/install.hpp"

#include <filesystem>
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
[[nodiscard]] StartupPreflightResult
run_sdl_startup_preflight(const std::filesystem::path &data_path);

} // namespace off::platform
