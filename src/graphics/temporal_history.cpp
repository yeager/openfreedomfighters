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
  in_flight_ = false;
  if (generation_ != std::numeric_limits<std::uint64_t>::max())
    ++generation_;
  return true;
}

std::optional<TemporalHistoryFrame>
TemporalHistoryLifecycle::begin_frame() noexcept {
  if (!target_ || in_flight_)
    return std::nullopt;
  in_flight_ = true;
  return TemporalHistoryFrame{.history_slot = newest_slot_,
                              .output_slot = static_cast<std::uint8_t>(newest_slot_ ^ 1U),
                              .history_valid = history_valid_,
                              .generation = generation_};
}

bool TemporalHistoryLifecycle::commit_frame() noexcept {
  if (!in_flight_)
    return false;
  newest_slot_ ^= 1U;
  history_valid_ = true;
  in_flight_ = false;
  return true;
}

void TemporalHistoryLifecycle::cancel_frame() noexcept { in_flight_ = false; }

void TemporalHistoryLifecycle::invalidate() noexcept {
  if (!target_)
    return;
  history_valid_ = false;
}

std::optional<TemporalHistoryTarget> TemporalHistoryLifecycle::target() const noexcept {
  return target_;
}

} // namespace off::graphics
