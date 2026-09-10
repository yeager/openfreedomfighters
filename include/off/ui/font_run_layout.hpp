#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace off::ui {

// Raster dimensions and ascent reported by the active font renderer for one
// already-selected LTR fallback run. Values are in UI pixels.
struct FontRunRasterMetrics {
  int width{};
  int height{};
  int ascent{};
  bool operator==(const FontRunRasterMetrics &) const = default;
};

struct FontRunPlacement {
  float x{};
  float y{};
  float width{};
  float height{};
  bool operator==(const FontRunPlacement &) const = default;
};

// Places byte-order LTR fallback runs on one shared baseline. `top` remains
// the requested top edge for the line's highest ascent. Invalid dimensions,
// ascents, or non-finite placement values reject the entire line.
[[nodiscard]] std::optional<std::vector<FontRunPlacement>>
layout_ltr_font_runs(float x, float top,
                     std::span<const FontRunRasterMetrics> runs) noexcept;

} // namespace off::ui
