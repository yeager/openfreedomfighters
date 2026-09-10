#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
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

// Returns a single embedded font capable of every scalar in independently
// authored UTF-8 UI text. It does not extract, persist, shape, or substitute
// retail font data; absent coverage is an explicit admission failure.
[[nodiscard]] std::optional<std::size_t>
select_font_for_utf8(const RetailUiFontSet &fonts, std::string_view text) noexcept;

} // namespace off::ui
