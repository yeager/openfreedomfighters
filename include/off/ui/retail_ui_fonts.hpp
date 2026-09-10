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

// One UTF-8 byte-range whose scalar values are covered by one verified font.
// It is a glyph-coverage result, not shaped text or a bidi layout run.
struct RetailUiFontRun {
  std::size_t font_index{};
  std::size_t byte_offset{};
  std::size_t byte_length{};
  bool operator==(const RetailUiFontRun&) const = default;
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

// Splits independently authored UTF-8 text into the smallest contiguous runs
// selected by first covering font. Missing/invalid scalar coverage rejects the
// whole request. This does not perform shaping, kerning, bidi reordering, or
// draw layout.
[[nodiscard]] std::optional<std::vector<RetailUiFontRun>>
select_font_runs_for_utf8(const RetailUiFontSet &fonts, std::string_view text) noexcept;

} // namespace off::ui
