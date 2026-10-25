#include "off/graphics/temporal_resolve_baseline.hpp"

namespace off::graphics {
namespace {

[[nodiscard]] constexpr bool valid(TemporalResolveExtent extent) noexcept {
  return extent.width != 0U && extent.height != 0U;
}

[[nodiscard]] constexpr bool identified(std::uint64_t resource) noexcept {
  return resource != 0U;
}

} // namespace

bool TemporalResolveBaseline::configure(TemporalHistoryTarget target) noexcept {
  const bool reallocate = history_.configure(target);
  if (reallocate) {
    jitter_.reset();
    inputs_.invalidate();
    submitted_resolve_ = false;
  }
  return reallocate;
}

std::optional<TemporalResolveInputs> TemporalResolveBaseline::begin_frame(
    bool modern_mode, TemporalResolveExtent output_extent,
    TemporalResolveExtent internal_extent,
    const TemporalResolveResourceSet &resources) noexcept {
  // Reject overlapping attempts without perturbing the already pending frame,
  // history slot or jitter sequence. The owner must commit or cancel it first.
  if (frame_in_flight())
    return std::nullopt;
  if (!modern_mode || !valid(output_extent) || !valid(internal_extent) ||
      !identified(resources.color) || !identified(resources.depth) ||
      !identified(resources.motion_vectors) || !identified(resources.exposure) ||
      !identified(resources.reactive_mask) || !identified(resources.hudless_color) ||
      !identified(resources.history[0]) || !identified(resources.history[1]) ||
      !resources.motion_vectors_written) {
    cancel_submission();
    return std::nullopt;
  }
  const auto target = history_.target();
  if (!target || target->width != output_extent.width ||
      target->height != output_extent.height)
    return std::nullopt;
  const auto frame = history_.begin_frame();
  const auto jitter = jitter_.next(
      true, {output_extent.width, output_extent.height},
      {internal_extent.width, internal_extent.height});
  if (!frame || !jitter) {
    cancel_submission();
    return std::nullopt;
  }
  const auto history_resource = resources.history[frame->history_slot];
  TemporalResolveInputs inputs{
      .internal_extent = internal_extent,
      .output_extent = output_extent,
      .color_resource = resources.color,
      .depth_resource = resources.depth,
      .motion_vector_resource = resources.motion_vectors,
      .exposure_resource = resources.exposure,
      .reactive_mask_resource = resources.reactive_mask,
      .hudless_color_resource = resources.hudless_color,
      .history_resource = history_resource,
      .motion_vectors_written = resources.motion_vectors_written,
      .jitter_applied = true,
      .history_frame = *frame,
      .jitter = *jitter,
      .jitter_internal_extent = internal_extent,
      .jitter_output_extent = output_extent};
  if (!inputs_.begin(inputs)) {
    cancel_submission();
    return std::nullopt;
  }
  return inputs;
}

bool TemporalResolveBaseline::commit_submission() noexcept {
  const auto pending = inputs_.pending();
  // Publish history last.  A failed input transaction means the backend did
  // not retain the complete resolve contract, so its output must never become
  // usable history for a later frame.
  if (!pending || !inputs_.commit() ||
      !history_.commit_frame(pending->history_frame)) {
    cancel_submission();
    return false;
  }
  submitted_resolve_ = true;
  return true;
}

void TemporalResolveBaseline::cancel_submission() noexcept {
  inputs_.cancel();
  history_.cancel_frame();
  jitter_.reset();
}

void TemporalResolveBaseline::invalidate() noexcept {
  cancel_submission();
  history_.invalidate();
  submitted_resolve_ = false;
}

bool TemporalResolveBaseline::frame_in_flight() const noexcept {
  return inputs_.in_flight() || history_.frame_in_flight();
}

std::optional<TemporalResolveInputs>
TemporalResolveBaseline::pending() const noexcept {
  return inputs_.pending();
}

} // namespace off::graphics
