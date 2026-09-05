#include "off/ui/graphics_menu.hpp"

#include <stdexcept>
#include <utility>

namespace off::ui {

GraphicsMenuSession::GraphicsMenuSession(
    settings::GraphicsCapabilities capabilities)
    : capabilities_(capabilities) {
  const auto resolved =
      settings::resolve_graphics_settings(confirmed_requested_, capabilities_);
  if (!resolved.effective.has_value()) {
    throw std::invalid_argument(
        "graphics menu capabilities cannot resolve default settings");
  }
  confirmed_effective_ = *resolved.effective;
  live_effective_ = confirmed_effective_;
  draft_ = confirmed_requested_;
}

void GraphicsMenuSession::set_confirmed(
    settings::RequestedGraphicsSettings requested,
    settings::EffectiveGraphicsSettings effective) {
  if (phase_ != GraphicsMenuPhase::closed) {
    throw std::logic_error(
        "cannot replace confirmed settings while menu is open");
  }
  const auto resolved =
      settings::resolve_graphics_settings(requested, capabilities_);
  if (!resolved.effective.has_value() || *resolved.effective != effective) {
    throw std::invalid_argument(
        "confirmed graphics settings do not match current capabilities");
  }
  confirmed_requested_ = std::move(requested);
  confirmed_effective_ = std::move(effective);
  live_effective_ = confirmed_effective_;
  draft_ = confirmed_requested_;
}

GraphicsMenuEffect GraphicsMenuSession::handle_key(GraphicsMenuKey key,
                                                   bool key_down, bool repeat) {
  if (!key_down || repeat || key == GraphicsMenuKey::other) {
    return GraphicsMenuEffect::none;
  }
  if (key == GraphicsMenuKey::f10) {
    if (phase_ == GraphicsMenuPhase::closed) {
      draft_ = confirmed_requested_;
      validation_error_.reset();
      phase_ = GraphicsMenuPhase::editing;
      return GraphicsMenuEffect::opened;
    }
    if (phase_ == GraphicsMenuPhase::editing) {
      draft_ = confirmed_requested_;
      validation_error_.reset();
      phase_ = GraphicsMenuPhase::closed;
      return GraphicsMenuEffect::closed;
    }
    return GraphicsMenuEffect::none;
  }
  if (phase_ == GraphicsMenuPhase::closed) {
    return GraphicsMenuEffect::quit_requested;
  }
  return cancel_or_revert();
}

std::optional<GraphicsApplyProposal> GraphicsMenuSession::request_apply() {
  if (phase_ != GraphicsMenuPhase::editing) {
    throw std::logic_error("graphics apply requested outside editing phase");
  }
  const auto resolution =
      settings::resolve_graphics_settings(draft_, capabilities_);
  validation_error_ = resolution.error;
  if (!resolution.effective.has_value()) {
    return std::nullopt;
  }
  pending_ = GraphicsApplyProposal{
      .requested = draft_,
      .effective = *resolution.effective,
      .display_confirmation_required = settings::requires_display_confirmation(
          confirmed_effective_, *resolution.effective),
  };
  phase_ = GraphicsMenuPhase::applying;
  return pending_;
}

GraphicsMenuEffect
GraphicsMenuSession::acknowledge_apply(bool success,
                                       GraphicsClock::time_point now) {
  if (phase_ != GraphicsMenuPhase::applying || !pending_.has_value()) {
    throw std::logic_error("stale graphics apply acknowledgement");
  }
  if (!success) {
    phase_ = GraphicsMenuPhase::editing;
    return GraphicsMenuEffect::none;
  }
  live_effective_ = pending_->effective;
  if (!pending_->display_confirmation_required) {
    confirmed_requested_ = pending_->requested;
    confirmed_effective_ = pending_->effective;
    pending_.reset();
    phase_ = GraphicsMenuPhase::closed;
    return GraphicsMenuEffect::commit_requested;
  }
  confirmation_deadline_ = now + std::chrono::seconds{15};
  phase_ = GraphicsMenuPhase::confirming;
  return GraphicsMenuEffect::none;
}

GraphicsMenuEffect GraphicsMenuSession::confirm() {
  if (phase_ != GraphicsMenuPhase::confirming || !pending_.has_value()) {
    throw std::logic_error("graphics confirmation requested in wrong phase");
  }
  confirmed_requested_ = pending_->requested;
  confirmed_effective_ = pending_->effective;
  pending_.reset();
  confirmation_deadline_.reset();
  phase_ = GraphicsMenuPhase::closed;
  return GraphicsMenuEffect::commit_requested;
}

GraphicsMenuEffect GraphicsMenuSession::cancel_or_revert() {
  if (phase_ == GraphicsMenuPhase::editing) {
    draft_ = confirmed_requested_;
    validation_error_.reset();
    phase_ = GraphicsMenuPhase::closed;
    return GraphicsMenuEffect::closed;
  }
  if (phase_ == GraphicsMenuPhase::confirming) {
    phase_ = GraphicsMenuPhase::reverting;
    return GraphicsMenuEffect::revert_requested;
  }
  return GraphicsMenuEffect::none;
}

GraphicsMenuEffect GraphicsMenuSession::tick(GraphicsClock::time_point now) {
  if (phase_ == GraphicsMenuPhase::confirming &&
      confirmation_deadline_.has_value() && now >= *confirmation_deadline_) {
    phase_ = GraphicsMenuPhase::reverting;
    return GraphicsMenuEffect::revert_requested;
  }
  return GraphicsMenuEffect::none;
}

GraphicsMenuEffect GraphicsMenuSession::acknowledge_revert(bool success) {
  if (phase_ != GraphicsMenuPhase::reverting) {
    throw std::logic_error("stale graphics revert acknowledgement");
  }
  if (!success) {
    phase_ = GraphicsMenuPhase::editing;
    return GraphicsMenuEffect::none;
  }
  live_effective_ = confirmed_effective_;
  draft_ = confirmed_requested_;
  pending_.reset();
  confirmation_deadline_.reset();
  phase_ = GraphicsMenuPhase::closed;
  return GraphicsMenuEffect::closed;
}

} // namespace off::ui
