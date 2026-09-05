#include "off/ui/graphics_menu.hpp"

#include <array>
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
      selected_row_ = GraphicsMenuRow::profile;
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
    return key == GraphicsMenuKey::escape ? GraphicsMenuEffect::quit_requested
                                          : GraphicsMenuEffect::none;
  }
  if (key == GraphicsMenuKey::escape) {
    return cancel_or_revert();
  }
  if (phase_ != GraphicsMenuPhase::editing) {
    return GraphicsMenuEffect::none;
  }
  constexpr std::array rows{
      GraphicsMenuRow::profile,      GraphicsMenuRow::window_mode,
      GraphicsMenuRow::window_size,  GraphicsMenuRow::present_mode,
      GraphicsMenuRow::apply,        GraphicsMenuRow::cancel,
      GraphicsMenuRow::render_scale, GraphicsMenuRow::upscaler,
      GraphicsMenuRow::shadows,      GraphicsMenuRow::defaults};
  std::size_t index = 0;
  for (std::size_t i = 0; i < rows.size(); ++i)
    if (rows[i] == selected_row_)
      index = i;
  if (key == GraphicsMenuKey::up) {
    index = (index + rows.size() - 1) % rows.size();
    selected_row_ = rows[index];
    return GraphicsMenuEffect::none;
  }
  if (key == GraphicsMenuKey::down) {
    selected_row_ = rows[(index + 1) % rows.size()];
    return GraphicsMenuEffect::none;
  }
  if (key == GraphicsMenuKey::enter || key == GraphicsMenuKey::space) {
    if (selected_row_ == GraphicsMenuRow::apply) {
      return GraphicsMenuEffect::apply_requested;
    }
    if (selected_row_ == GraphicsMenuRow::cancel) {
      return cancel_or_revert();
    }
    if (selected_row_ == GraphicsMenuRow::defaults) {
      draft_ = settings::RequestedGraphicsSettings{};
      validation_error_.reset();
      return GraphicsMenuEffect::none;
    }
  }
  if (key != GraphicsMenuKey::left && key != GraphicsMenuKey::right) {
    return GraphicsMenuEffect::none;
  }
  const auto forward = key == GraphicsMenuKey::right;
  switch (selected_row_) {
  case GraphicsMenuRow::profile: {
    unsigned value =
        draft_.profile == Mode::original ? 0U : (draft_.modern_plus ? 2U : 1U);
    value = forward ? (value + 1U) % 3U : (value + 2U) % 3U;
    draft_.profile = value == 0U ? Mode::original : Mode::modern;
    draft_.modern_plus = value == 2U;
    break;
  }
  case GraphicsMenuRow::window_mode:
    draft_.window_mode = draft_.window_mode == settings::WindowMode::windowed
                             ? settings::WindowMode::borderless_desktop
                             : settings::WindowMode::windowed;
    break;
  case GraphicsMenuRow::window_size: {
    constexpr std::array sizes{
        settings::WindowSize{1280, 720}, settings::WindowSize{1920, 1080},
        settings::WindowSize{2560, 1440}, settings::WindowSize{3840, 2160}};
    std::size_t size_index = 0;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
      if (sizes[i] == draft_.windowed_size) {
        size_index = i;
      }
    }
    size_index = forward ? (size_index + 1) % sizes.size()
                         : (size_index + sizes.size() - 1) % sizes.size();
    draft_.windowed_size = sizes[size_index];
    break;
  }
  case GraphicsMenuRow::present_mode: {
    auto value = static_cast<unsigned>(draft_.present_mode);
    value = forward ? (value + 1U) % 3U : (value + 2U) % 3U;
    draft_.present_mode = static_cast<settings::PresentMode>(value);
    break;
  }
  case GraphicsMenuRow::render_scale: {
    constexpr std::array<std::uint16_t, 7> scales{50,  67,  75, 100,
                                                  125, 150, 200};
    std::size_t scale_index = 3;
    for (std::size_t i = 0; i < scales.size(); ++i)
      if (scales[i] == draft_.render_scale_percent)
        scale_index = i;
    scale_index = forward ? (scale_index + 1) % scales.size()
                          : (scale_index + scales.size() - 1) % scales.size();
    draft_.render_scale_percent = scales[scale_index];
    break;
  }
  case GraphicsMenuRow::upscaler: {
    auto value = static_cast<unsigned>(draft_.upscaler);
    value = forward ? (value + 1U) % 3U : (value + 2U) % 3U;
    draft_.upscaler = static_cast<settings::Upscaler>(value);
    break;
  }
  case GraphicsMenuRow::shadows: {
    auto value = static_cast<unsigned>(draft_.shadow_quality);
    value = forward ? (value + 1U) % 3U : (value + 2U) % 3U;
    draft_.shadow_quality = static_cast<settings::ShadowQuality>(value);
    break;
  }
  case GraphicsMenuRow::apply:
  case GraphicsMenuRow::cancel:
  case GraphicsMenuRow::defaults:
    break;
  }
  validation_error_.reset();
  return GraphicsMenuEffect::none;
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
