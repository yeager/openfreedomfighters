#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace off::graphics {

// Output is the presentation extent; internal is the extent that receives the
// jittered projection.  Keeping both in a sample prevents a provider boundary
// from silently interpreting internal-pixel offsets as output-pixel offsets.
struct TemporalJitterExtent {
  std::uint32_t width{};
  std::uint32_t height{};
  [[nodiscard]] bool operator==(const TemporalJitterExtent &) const = default;
};

struct TemporalJitterSample {
  std::uint64_t sequence_index{};
  std::uint64_t generation{};
  // Centered subpixel displacement in internal-render pixels.
  std::array<float, 2> internal_pixel_offset{};
  // NDC translations for a projection rendered at internal extent and for a
  // provider which works at the presentation extent, respectively.  Y is
  // inverted because raster pixel coordinates grow downward and clip Y upward.
  std::array<float, 2> internal_ndc_offset{};
  std::array<float, 2> output_ndc_offset{};
  [[nodiscard]] bool operator==(const TemporalJitterSample &) const = default;
};

// Deterministic Modern-only camera-jitter provider. This is deliberately only
// a projection-input contract: it does not create motion vectors, resolve
// history, or advertise temporal/vendor upscaling. The caller must reset it on
// camera/scene discontinuities and apply internal_ndc_offset exactly once to a
// clear, per-frame projection before recording draws.
class TemporalJitterProvider {
public:
  // Original mode returns no sample and resets the sequence, preserving its
  // unjittered projection. Invalid extents also fail closed and reset.
  [[nodiscard]] std::optional<TemporalJitterSample>
  next(bool modern_mode, TemporalJitterExtent output,
       TemporalJitterExtent internal) noexcept;

  // Camera cuts, scene replacement, projection changes and failed submission
  // paths must discard the current sequence before the next Modern frame.
  void reset() noexcept;

  [[nodiscard]] std::optional<TemporalJitterExtent> output_extent() const noexcept;
  [[nodiscard]] std::optional<TemporalJitterExtent> internal_extent() const noexcept;

private:
  std::optional<TemporalJitterExtent> output_;
  std::optional<TemporalJitterExtent> internal_;
  std::uint64_t next_sequence_index_{};
  std::uint64_t generation_{};
};

} // namespace off::graphics
