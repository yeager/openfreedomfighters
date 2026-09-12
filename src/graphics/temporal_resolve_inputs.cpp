#include "off/graphics/temporal_resolve_inputs.hpp"

namespace off::graphics {
namespace {

[[nodiscard]] constexpr bool valid(TemporalResolveExtent extent) noexcept {
  return extent.width != 0U && extent.height != 0U;
}

[[nodiscard]] constexpr bool identified(std::uint64_t resource) noexcept {
  return resource != 0U;
}

[[nodiscard]] bool complete(const TemporalResolveInputs &inputs) noexcept {
  // The history frame's output slot is always one of the two lifecycle slots.
  if (inputs.history_frame.history_slot > 1U || inputs.history_frame.output_slot > 1U ||
      inputs.history_frame.history_slot == inputs.history_frame.output_slot)
    return false;
  return valid(inputs.internal_extent) && valid(inputs.output_extent) &&
         identified(inputs.color_resource) && identified(inputs.depth_resource) &&
         identified(inputs.motion_vector_resource) &&
         identified(inputs.exposure_resource) && identified(inputs.reactive_mask_resource) &&
         identified(inputs.hudless_color_resource) && identified(inputs.history_resource) &&
         inputs.motion_vectors_written && inputs.jitter_applied &&
         inputs.jitter_internal_extent == inputs.internal_extent &&
         inputs.jitter_output_extent == inputs.output_extent;
}

} // namespace

bool TemporalResolveInputLifecycle::begin(TemporalResolveInputs inputs) noexcept {
  if (in_flight_ || !complete(inputs))
    return false;
  pending_ = inputs;
  in_flight_ = true;
  return true;
}

bool TemporalResolveInputLifecycle::commit() noexcept {
  if (!in_flight_ || !pending_)
    return false;
  pending_.reset();
  in_flight_ = false;
  return true;
}

void TemporalResolveInputLifecycle::cancel() noexcept {
  pending_.reset();
  in_flight_ = false;
}

void TemporalResolveInputLifecycle::invalidate() noexcept { cancel(); }

} // namespace off::graphics
