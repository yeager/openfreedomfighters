#pragma once

#include "off/settings/graphics_settings.hpp"

#include <filesystem>
#include <optional>

namespace off::settings {

// The on-disk document contains only RequestedGraphicsSettings.  In
// particular, it never records renderer fallbacks or machine capabilities.
enum class GraphicsSettingsLoadStatus : unsigned char {
  missing,
  loaded,
  invalid,
  io_error,
};

struct GraphicsSettingsLoadResult {
  GraphicsSettingsLoadStatus status{GraphicsSettingsLoadStatus::missing};
  std::optional<RequestedGraphicsSettings> settings;
};

// A small, deliberately strict, versioned text format.  Invalid, unsupported,
// or extended documents are reported as invalid and are left untouched.
[[nodiscard]] GraphicsSettingsLoadResult
load_graphics_settings(const std::filesystem::path &path);

// Writes to a sibling temporary file and atomically replaces path only after a
// complete, valid document has been written. A false result before replacement
// leaves the prior path intact; a post-replacement directory-sync failure can
// report false even though the new complete document is already visible.
[[nodiscard]] bool
save_graphics_settings(const std::filesystem::path &path,
                       const RequestedGraphicsSettings &settings);

} // namespace off::settings
