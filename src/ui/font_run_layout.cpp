#include "off/ui/font_run_layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace off::ui {

std::optional<std::vector<FontRunPlacement>>
layout_ltr_font_runs(float x, float top,
                     std::span<const FontRunRasterMetrics> runs) noexcept {
  if (!std::isfinite(x) || !std::isfinite(top))
    return std::nullopt;
  int maximum_ascent = 0;
  for (const auto &run : runs) {
    if (run.width <= 0 || run.height <= 0 || run.ascent < 0)
      return std::nullopt;
    maximum_ascent = std::max(maximum_ascent, run.ascent);
  }
  const float baseline = top + static_cast<float>(maximum_ascent);
  if (!std::isfinite(baseline))
    return std::nullopt;
  std::vector<FontRunPlacement> placements;
  placements.reserve(runs.size());
  float cursor = x;
  for (const auto &run : runs) {
    const auto width = static_cast<float>(run.width);
    const auto height = static_cast<float>(run.height);
    const auto run_top = baseline - static_cast<float>(run.ascent);
    if (!std::isfinite(cursor) || !std::isfinite(run_top) ||
        cursor + width > std::numeric_limits<float>::max())
      return std::nullopt;
    placements.push_back({cursor, run_top, width, height});
    cursor += width;
  }
  return placements;
}

} // namespace off::ui
