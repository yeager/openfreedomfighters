#include "off/graphics/temporal_jitter.hpp"

#include <limits>

namespace off::graphics {
namespace {

[[nodiscard]] bool valid(TemporalJitterExtent extent) noexcept {
  return extent.width != 0U && extent.height != 0U;
}

[[nodiscard]] float halton(std::uint64_t index, std::uint32_t base) noexcept {
  float result = 0.0F;
  float factor = 1.0F;
  while (index != 0U) {
    factor /= static_cast<float>(base);
    result += factor * static_cast<float>(index % base);
    index /= base;
  }
  return result;
}

} // namespace

std::optional<TemporalJitterSample>
TemporalJitterProvider::next(bool modern_mode, TemporalJitterExtent output,
                             TemporalJitterExtent internal) noexcept {
  if (!modern_mode || !valid(output) || !valid(internal)) {
    reset();
    return std::nullopt;
  }
  if (output_ != output || internal_ != internal) {
    output_ = output;
    internal_ = internal;
    next_sequence_index_ = 0U;
    if (generation_ != std::numeric_limits<std::uint64_t>::max())
      ++generation_;
  }
  // Index zero is reserved as the reset marker; the first usable Halton point
  // is index one and remains stable across all platforms.
  const auto sequence_index = next_sequence_index_ + 1U;
  const std::array pixel{halton(sequence_index, 2U) - 0.5F,
                         halton(sequence_index, 3U) - 0.5F};
  if (next_sequence_index_ != std::numeric_limits<std::uint64_t>::max())
    ++next_sequence_index_;
  const auto ndc_for = [&](TemporalJitterExtent extent) {
    return std::array{2.0F * pixel[0] / static_cast<float>(extent.width),
                      -2.0F * pixel[1] / static_cast<float>(extent.height)};
  };
  return TemporalJitterSample{.sequence_index = sequence_index,
                              .generation = generation_,
                              .internal_pixel_offset = pixel,
                              .internal_ndc_offset = ndc_for(internal),
                              .output_ndc_offset = ndc_for(output)};
}

void TemporalJitterProvider::reset() noexcept {
  output_.reset();
  internal_.reset();
  next_sequence_index_ = 0U;
  if (generation_ != std::numeric_limits<std::uint64_t>::max())
    ++generation_;
}

std::optional<TemporalJitterExtent>
TemporalJitterProvider::output_extent() const noexcept { return output_; }

std::optional<TemporalJitterExtent>
TemporalJitterProvider::internal_extent() const noexcept { return internal_; }

} // namespace off::graphics
