#pragma once

#include "off/settings/graphics_settings.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace off::ui {

using GraphicsClock = std::chrono::steady_clock;

enum class GraphicsMenuPhase : std::uint8_t {
  closed,
  editing,
  applying,
  confirming,
  reverting
};
enum class GraphicsMenuKey : std::uint8_t {
  f10,
  escape,
  up,
  down,
  left,
  right,
  enter,
  space,
  other
};
enum class GraphicsMenuRow : std::uint8_t {
  profile,
  window_mode,
  window_size,
  present_mode,
  apply,
  cancel
};
enum class GraphicsMenuEffect : std::uint8_t {
  none,
  opened,
  closed,
  quit_requested,
  apply_requested,
  revert_requested,
  commit_requested
};

struct GraphicsApplyProposal {
  settings::RequestedGraphicsSettings requested;
  settings::EffectiveGraphicsSettings effective;
  bool display_confirmation_required{false};
};

class GraphicsMenuSession final {
public:
  explicit GraphicsMenuSession(settings::GraphicsCapabilities capabilities);

  void set_confirmed(settings::RequestedGraphicsSettings requested,
                     settings::EffectiveGraphicsSettings effective);

  [[nodiscard]] GraphicsMenuEffect handle_key(GraphicsMenuKey key,
                                              bool key_down, bool repeat);
  [[nodiscard]] std::optional<GraphicsApplyProposal> request_apply();
  [[nodiscard]] GraphicsMenuEffect
  acknowledge_apply(bool success, GraphicsClock::time_point now);
  [[nodiscard]] GraphicsMenuEffect confirm();
  [[nodiscard]] GraphicsMenuEffect cancel_or_revert();
  [[nodiscard]] GraphicsMenuEffect tick(GraphicsClock::time_point now);
  [[nodiscard]] GraphicsMenuEffect acknowledge_revert(bool success);

  [[nodiscard]] GraphicsMenuPhase phase() const noexcept { return phase_; }
  [[nodiscard]] GraphicsMenuRow selected_row() const noexcept {
    return selected_row_;
  }
  [[nodiscard]] const settings::RequestedGraphicsSettings &
  confirmed_requested() const noexcept {
    return confirmed_requested_;
  }
  [[nodiscard]] const settings::EffectiveGraphicsSettings &
  confirmed_effective() const noexcept {
    return confirmed_effective_;
  }
  [[nodiscard]] const settings::EffectiveGraphicsSettings &
  live_effective() const noexcept {
    return live_effective_;
  }
  [[nodiscard]] settings::RequestedGraphicsSettings &draft() noexcept {
    return draft_;
  }
  [[nodiscard]] const settings::RequestedGraphicsSettings &draft() const noexcept {
    return draft_;
  }
  [[nodiscard]] std::optional<settings::GraphicsValidationError>
  validation_error() const noexcept {
    return validation_error_;
  }
  [[nodiscard]] std::optional<GraphicsClock::time_point>
  confirmation_deadline() const noexcept {
    return confirmation_deadline_;
  }

private:
  settings::GraphicsCapabilities capabilities_;
  GraphicsMenuPhase phase_{GraphicsMenuPhase::closed};
  settings::RequestedGraphicsSettings confirmed_requested_;
  settings::EffectiveGraphicsSettings confirmed_effective_;
  settings::EffectiveGraphicsSettings live_effective_;
  settings::RequestedGraphicsSettings draft_;
  std::optional<GraphicsApplyProposal> pending_;
  std::optional<settings::GraphicsValidationError> validation_error_;
  std::optional<GraphicsClock::time_point> confirmation_deadline_;
  GraphicsMenuRow selected_row_{GraphicsMenuRow::profile};
};

} // namespace off::ui
