#include "off/graphics/render_scale.hpp"

#include <limits>

namespace off::graphics {
namespace {

[[nodiscard]] std::optional<std::uint32_t>
scaled_dimension(std::uint32_t extent, std::uint16_t percent) noexcept {
  const auto product = static_cast<std::uint64_t>(extent) * percent;
  const auto rounded = (product + 99U) / 100U;
  if (rounded == 0U || rounded > std::numeric_limits<std::uint32_t>::max())
    return std::nullopt;
  return static_cast<std::uint32_t>(rounded);
}

} // namespace

std::optional<RenderScaleExtent>
resolve_render_scale_extent(RenderScaleExtent output,
                            std::uint16_t percent) noexcept {
  if (output.width == 0U || output.height == 0U || percent < 50U ||
      percent > 200U)
    return std::nullopt;
  const auto width = scaled_dimension(output.width, percent);
  const auto height = scaled_dimension(output.height, percent);
  if (!width || !height)
    return std::nullopt;
  return RenderScaleExtent{*width, *height};
}

} // namespace off::graphics
