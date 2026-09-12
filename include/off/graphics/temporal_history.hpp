#pragma once

#include <cstdint>
#include <optional>

namespace off::graphics {

// Opaque render-target identity. The platform backend owns the corresponding
// API format; this state machine only protects temporal-history lifetime.
struct TemporalHistoryTarget {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t format{};
  [[nodiscard]] bool operator==(const TemporalHistoryTarget &) const = default;
};

struct TemporalHistoryFrame {
  std::uint8_t history_slot{};
  std::uint8_t output_slot{};
  bool history_valid{};
  std::uint64_t generation{};
  [[nodiscard]] bool operator==(const TemporalHistoryFrame &) const = default;
};

// Owns the platform-independent double-buffer contract for a temporal color
// history. It intentionally does not claim motion, depth, jitter, or execute
// an upscaler: a backend may advertise temporal upscaling only after it binds
// those inputs and submits this frame to a real provider.
class TemporalHistoryLifecycle {
public:
  // Returns true when the backend must (re)allocate both history targets.
  // Invalid descriptors are rejected without changing live state.
  [[nodiscard]] bool configure(TemporalHistoryTarget target) noexcept;

  // Starts one submission. A frame cannot overlap another frame. The first
  // frame after configuration or invalidation has no usable history.
  [[nodiscard]] std::optional<TemporalHistoryFrame> begin_frame() noexcept;

  // Makes `frame`'s output slot the next frame's history only after the
  // backend has successfully submitted that exact GPU work.  The frame token
  // prevents a delayed completion from an invalidated or cancelled submission
  // from publishing a newer in-flight frame.
  [[nodiscard]] bool commit_frame(const TemporalHistoryFrame &frame) noexcept;
  void cancel_frame() noexcept;

  // Camera cuts, scene replacement, and other discontinuities must call this
  // before the next begin_frame. It retains allocated targets but discards
  // their temporal meaning.
  void invalidate() noexcept;

  [[nodiscard]] std::optional<TemporalHistoryTarget> target() const noexcept;
  [[nodiscard]] bool frame_in_flight() const noexcept {
    return in_flight_frame_.has_value();
  }

private:
  std::optional<TemporalHistoryTarget> target_;
  std::uint8_t newest_slot_{};
  bool history_valid_{};
  std::optional<TemporalHistoryFrame> in_flight_frame_;
  std::uint64_t generation_{};
};

} // namespace off::graphics
