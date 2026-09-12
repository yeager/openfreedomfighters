#include "off/graphics/temporal_jitter.hpp"

#include <cmath>
#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
bool near(float left, float right) { return std::abs(left - right) < 1.0e-7F; }
} // namespace

int main() {
  using off::graphics::TemporalJitterExtent;
  using off::graphics::TemporalJitterProvider;
  TemporalJitterProvider jitter;
  constexpr TemporalJitterExtent output{1920U, 1080U};
  constexpr TemporalJitterExtent internal{960U, 540U};
  check(!jitter.next(false, output, internal),
        "Original mode receives no jitter sample");
  const auto first = jitter.next(true, output, internal);
  check(first && first->sequence_index == 1U && near(first->internal_pixel_offset[0], 0.0F) &&
            near(first->internal_pixel_offset[1], -1.0F / 6.0F),
        "first Modern sample is the deterministic centered Halton point");
  check(first && near(first->internal_ndc_offset[0], 0.0F) &&
            near(first->internal_ndc_offset[1], 1.0F / 1620.0F) &&
            near(first->output_ndc_offset[1], 1.0F / 3240.0F),
        "internal and output NDC offsets normalize the same pixel displacement independently");
  const auto second = jitter.next(true, output, internal);
  check(second && second->sequence_index == 2U &&
            near(second->internal_pixel_offset[0], -0.25F) &&
            near(second->internal_pixel_offset[1], 1.0F / 6.0F),
        "sequence advances deterministically once per Modern frame");
  jitter.reset();
  const auto reset = jitter.next(true, output, internal);
  check(reset && reset->sequence_index == 1U && reset->generation > first->generation &&
            reset->internal_pixel_offset == first->internal_pixel_offset,
        "explicit discontinuity restarts the stable sequence in a new generation");
  const auto resized = jitter.next(true, {2560U, 1440U}, {1280U, 720U});
  check(resized && resized->sequence_index == 1U && resized->generation > reset->generation &&
            near(resized->internal_ndc_offset[1], 1.0F / 2160.0F),
        "extent change resets sequence and recomputes internal normalization");
  check(!jitter.next(true, {0U, 1U}, internal) && !jitter.output_extent() &&
            !jitter.internal_extent(),
        "invalid dimensions fail closed without retaining a usable sequence");
  return failures == 0 ? 0 : 1;
}
