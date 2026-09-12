#pragma once

#include "off/graphics/temporal_history.hpp"
#include "off/graphics/temporal_jitter.hpp"
#include "off/graphics/temporal_resolve_inputs.hpp"

#include <cstdint>
#include <optional>

namespace off::graphics {

// Opaque identities of resources which an API backend has already created and
// populated for one frame.  The baseline deliberately cannot allocate or
// synthesize any of them: a zero identity keeps temporal resolution disabled.
struct TemporalResolveResourceSet {
  std::uint64_t color{};
  std::uint64_t depth{};
  std::uint64_t motion_vectors{};
  std::uint64_t exposure{};
  std::uint64_t reactive_mask{};
  std::uint64_t hudless_color{};
  std::uint64_t history[2]{};
  bool motion_vectors_written{};
};

// Backend-neutral transaction coordinator for the first portable temporal
// resolver.  A platform backend supplies resource identities only after it has
// allocated them and recorded its producer pass.  `commit_submission` is
// intentionally separate: calling it means the backend successfully submitted
// the real resolve pass, not merely that it prepared inputs.
//
// This is not a temporal filter and is not a vendor adapter. It composes the
// existing history, jitter and complete-input contracts so SDL/Vulkan/Metal
// backends have one fail-closed boundary before they advertise temporal.
class TemporalResolveBaseline final {
public:
  // Returns true only when a backend must allocate/recreate both history
  // surfaces. Invalid descriptors preserve the previous configuration.
  [[nodiscard]] bool configure(TemporalHistoryTarget target) noexcept;

  // Begins one Modern-frame temporal transaction. Original mode, absent
  // resources, invalid extents and overlapping frames all fail closed.
  [[nodiscard]] std::optional<TemporalResolveInputs>
  begin_frame(bool modern_mode, TemporalResolveExtent output_extent,
              TemporalResolveExtent internal_extent,
              const TemporalResolveResourceSet &resources) noexcept;

  // Call only after the actual API resolve pass was accepted for submission.
  // The first successful call is the sole evidence this coordinator exposes
  // for a portable-temporal runtime binding.
  [[nodiscard]] bool commit_submission() noexcept;
  void cancel_submission() noexcept;
  void invalidate() noexcept;

  [[nodiscard]] bool frame_in_flight() const noexcept;
  [[nodiscard]] bool has_submitted_resolve() const noexcept {
    return submitted_resolve_;
  }
  [[nodiscard]] std::optional<TemporalResolveInputs> pending() const noexcept;

private:
  TemporalHistoryLifecycle history_;
  TemporalJitterProvider jitter_;
  TemporalResolveInputLifecycle inputs_;
  bool submitted_resolve_{};
};

} // namespace off::graphics
