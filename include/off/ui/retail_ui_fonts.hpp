#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace off::ui {

struct RetailUiFont {
  std::vector<std::byte> sfnt;
};

struct RetailUiFontSet {
  std::vector<RetailUiFont> fonts;
};

// Loads every structurally valid embedded font from the startup scene archive.
// Bytes remain owned by the returned set and are never written to disk.
[[nodiscard]] RetailUiFontSet
load_retail_ui_fonts(const std::filesystem::path &startup_archive);

[[nodiscard]] bool is_bounded_sfnt(std::span<const std::byte> bytes) noexcept;

} // namespace off::ui
