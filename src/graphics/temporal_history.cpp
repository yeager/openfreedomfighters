#include "off/graphics/temporal_history.hpp"

#include <limits>

namespace off::graphics {

bool TemporalHistoryLifecycle::configure(TemporalHistoryTarget target) noexcept {
  if (target.width == 0U || target.height == 0U || target.format == 0U)
    return false;
  if (target_ && *target_ == target)
    return false;
  target_ = target;
  newest_slot_ = 0U;
  history_valid_ = false;
  in_flight_frame_.reset();
  if (generation_ != std::numeric_limits<std::uint64_t>::max())
    ++generation_;
  return true;
}

std::optional<TemporalHistoryFrame>
TemporalHistoryLifecycle::begin_frame() noexcept {
  if (!target_ || in_flight_frame_ ||
      generation_ == std::numeric_limits<std::uint64_t>::max())
    return std::nullopt;
  // This generation is a submission token, not merely a target epoch.  A
  // cancelled frame and its replacement can otherwise have the same slots and
  // epoch, allowing a late GPU completion to publish the replacement.
  ++generation_;
  in_flight_frame_ = TemporalHistoryFrame{
      .history_slot = newest_slot_,
      .output_slot = static_cast<std::uint8_t>(newest_slot_ ^ 1U),
      .history_valid = history_valid_,
      .generation = generation_};
  return in_flight_frame_;
}

bool TemporalHistoryLifecycle::commit_frame(const TemporalHistoryFrame &frame) noexcept {
  if (!in_flight_frame_ || *in_flight_frame_ != frame)
    return false;
  newest_slot_ = frame.output_slot;
  history_valid_ = true;
  in_flight_frame_.reset();
  return true;
}

void TemporalHistoryLifecycle::cancel_frame() noexcept { in_flight_frame_.reset(); }

void TemporalHistoryLifecycle::invalidate() noexcept {
  if (!target_)
    return;
  // A discontinuity may be discovered after a frame has been prepared but
  // before its GPU submission is accepted.  That prepared output belongs to
  // the old view and must never become valid history for the new one.
  cancel_frame();
  history_valid_ = false;
  if (generation_ != std::numeric_limits<std::uint64_t>::max())
    ++generation_;
}

std::optional<TemporalHistoryTarget> TemporalHistoryLifecycle::target() const noexcept {
  return target_;
}

} // namespace off::graphics
