#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace off::ui {

// A UTF-8 byte span that is safe to hand intact to a future shaping engine.
// It is deliberately not a glyph run: advances, script selection, bidi
// resolution, fallback and rasterization require HarfBuzz/FreeType/FriBidi.
struct TextCluster {
  std::size_t byte_offset{};
  std::size_t byte_length{};
  std::size_t scalar_count{};
  bool operator==(const TextCluster &) const = default;
};

enum class TextBaseDirection { left_to_right, right_to_left, neutral };

struct TextLayoutBoundary {
  TextBaseDirection base_direction{TextBaseDirection::neutral};
  std::vector<TextCluster> clusters;
};

// Validates bounded Unicode-scalar UTF-8 and preserves a deliberately bounded
// grapheme subset (combining marks, Hangul, CRLF, ZWJ emoji sequences and
// regional-indicator pairs) as indivisible byte spans. It does not claim full
// UAX #29 coverage: a generated Unicode property table is required before
// that claim can be made. This is an admission boundary, not a substitute for
// HarfBuzz shaping, FreeType metrics or FriBidi bidi layout.
[[nodiscard]] std::optional<TextLayoutBoundary>
make_text_layout_boundary(std::string_view utf8) noexcept;

} // namespace off::ui
