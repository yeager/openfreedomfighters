#include "off/graphics/render_scale.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  using off::graphics::RenderScaleExtent;
  using off::graphics::resolve_render_scale_extent;
  constexpr RenderScaleExtent output{1920U, 1080U};
  check(resolve_render_scale_extent(output, 50U) ==
            RenderScaleExtent{960U, 540U},
        "50 percent halves an ordinary output extent");
  check(resolve_render_scale_extent(output, 100U) == output,
        "100 percent preserves output dimensions");
  check(resolve_render_scale_extent(output, 125U) ==
            RenderScaleExtent{2400U, 1350U},
        "125 percent supports supersampling targets");
  check(resolve_render_scale_extent(output, 200U) ==
            RenderScaleExtent{3840U, 2160U},
        "200 percent doubles both dimensions");
  check(resolve_render_scale_extent({1U, 1U}, 50U) ==
            RenderScaleExtent{1U, 1U},
        "fractional dimensions round upward instead of becoming zero");
  check(!resolve_render_scale_extent({0U, 1U}, 100U) &&
            !resolve_render_scale_extent({1U, 0U}, 100U) &&
            !resolve_render_scale_extent(output, 49U) &&
            !resolve_render_scale_extent(output, 201U),
        "zero extents and out-of-contract percentages fail closed");
  check(!resolve_render_scale_extent(
            {std::numeric_limits<std::uint32_t>::max(), 1U}, 200U),
        "overflowing scaled dimensions fail closed");
  return failures == 0 ? 0 : 1;
}
