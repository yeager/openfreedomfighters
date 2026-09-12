#pragma once

#include "off/graphics/temporal_history.hpp"
#include "off/graphics/temporal_jitter.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {

// Resource metadata at the temporal-resolve boundary.  These are deliberately
// opaque identities rather than API handles: this contract validates that a
// real producer supplied every input without creating, sampling, or resolving
// any GPU resource itself.
struct TemporalResolveExtent {
  std::uint32_t width{};
  std::uint32_t height{};
  [[nodiscard]] bool operator==(const TemporalResolveExtent &) const = default;
};

struct TemporalResolveInputs {
  TemporalResolveExtent internal_extent{};
  TemporalResolveExtent output_extent{};
  std::uint64_t color_resource{};
  std::uint64_t depth_resource{};
  std::uint64_t motion_vector_resource{};
  std::uint64_t exposure_resource{};
  std::uint64_t reactive_mask_resource{};
  std::uint64_t hudless_color_resource{};
  std::uint64_t history_resource{};
  bool motion_vectors_written{};
  bool jitter_applied{};
  TemporalHistoryFrame history_frame{};
  TemporalJitterSample jitter{};
  // The producer records the extents against which it generated the sample;
  // TemporalJitterSample intentionally contains offsets only.
  TemporalResolveExtent jitter_internal_extent{};
  TemporalResolveExtent jitter_output_extent{};
  [[nodiscard]] bool operator==(const TemporalResolveInputs &) const = default;
};

// Transactional, source-free gate for a future temporal resolve pass.  A
// frame is usable only if actual producers identify every required resource,
// motion data was written (not merely allocated), and the supplied history and
// jitter records agree with the current frame's extents.  First frames are
// allowed with invalid *content* history, but still require a history target.
//
// This class does not manufacture vectors, infer exposure, bind resources, or
// enable temporal/vendor upscalers.  It exists so those operations have one
// fail-closed readiness boundary when a real resolve pass is introduced.
class TemporalResolveInputLifecycle final {
public:
  [[nodiscard]] bool begin(TemporalResolveInputs inputs) noexcept;
  [[nodiscard]] bool commit() noexcept;
  void cancel() noexcept;
  void invalidate() noexcept;

  [[nodiscard]] bool ready() const noexcept { return pending_.has_value(); }
  [[nodiscard]] bool in_flight() const noexcept { return in_flight_; }
  [[nodiscard]] std::optional<TemporalResolveInputs> pending() const noexcept {
    return pending_;
  }

private:
  std::optional<TemporalResolveInputs> pending_;
  bool in_flight_{};
};

} // namespace off::graphics
