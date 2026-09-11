#pragma once

#include <cstdint>
#include <optional>

namespace off::graphics {

struct RenderScaleExtent {
  std::uint32_t width{};
  std::uint32_t height{};
  [[nodiscard]] bool operator==(const RenderScaleExtent &) const = default;
};

// Resolves the dimensions of an internal render target. The percentage is an
// already-validated graphics setting, but the helper remains fail-closed for
// callers outside that boundary. Dimensions round upward so a nonzero source
// extent never loses its final pixel through integer truncation.
[[nodiscard]] std::optional<RenderScaleExtent>
resolve_render_scale_extent(RenderScaleExtent output,
                            std::uint16_t percent) noexcept;

} // namespace off::graphics
