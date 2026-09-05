#include "off/settings/graphics_settings.hpp"
#include "off/ui/graphics_menu.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  off::settings::GraphicsCapabilities capabilities;
  capabilities.mailbox_present = false;
  capabilities.immediate_present = false;

  off::settings::RequestedGraphicsSettings requested;
  requested.profile = off::Mode::modern;
  requested.window_mode = off::settings::WindowMode::borderless_desktop;
  requested.windowed_size = {1920, 1080};
  requested.present_mode = off::settings::PresentMode::mailbox;
  const auto requested_copy = requested;
  const auto resolution =
      off::settings::resolve_graphics_settings(requested, capabilities);
  check(resolution.effective.has_value() && requested == requested_copy &&
            resolution.effective->profile == off::Mode::modern &&
            resolution.effective->window_mode ==
                off::settings::WindowMode::borderless_desktop &&
            resolution.effective->present_mode ==
                off::settings::PresentMode::vsync &&
            resolution.effective->fallbacks.size() == 1 &&
            resolution.effective->fallbacks[0].reason ==
                off::settings::FallbackReason::mailbox_unavailable,
        "preserve requested settings and report a deterministic present "
        "fallback");

  auto unavailable = capabilities;
  unavailable.modern_profile = false;
  unavailable.borderless_desktop = false;
  const auto fallback =
      off::settings::resolve_graphics_settings(requested, unavailable);
  check(fallback.effective.has_value() &&
            fallback.effective->profile == off::Mode::original &&
            fallback.effective->window_mode ==
                off::settings::WindowMode::windowed &&
            fallback.effective->fallbacks.size() == 3 &&
            fallback.effective->fallbacks[0].field ==
                off::settings::GraphicsField::profile &&
            fallback.effective->fallbacks[1].field ==
                off::settings::GraphicsField::window_mode &&
            fallback.effective->fallbacks[2].field ==
                off::settings::GraphicsField::present_mode,
        "resolve independent fallbacks in stable field order");

  auto invalid = requested;
  invalid.windowed_size.width = 0;
  const auto invalid_resolution =
      off::settings::resolve_graphics_settings(invalid, capabilities);
  check(!invalid_resolution.effective.has_value() &&
            invalid_resolution.error ==
                off::settings::GraphicsValidationError::zero_window_dimension,
        "reject a zero output dimension without partially resolving settings");
  invalid = requested;
  invalid.window_mode = static_cast<off::settings::WindowMode>(255);
  check(off::settings::resolve_graphics_settings(invalid, capabilities).error ==
            off::settings::GraphicsValidationError::invalid_enum,
        "reject an invalid graphics enum representation");

  off::ui::GraphicsMenuSession menu{capabilities};
  const auto baseline = menu.confirmed_requested();
  check(menu.handle_key(off::ui::GraphicsMenuKey::f10, false, false) ==
                off::ui::GraphicsMenuEffect::none &&
            menu.handle_key(off::ui::GraphicsMenuKey::f10, true, true) ==
                off::ui::GraphicsMenuEffect::none &&
            menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false) ==
                off::ui::GraphicsMenuEffect::opened,
        "open the graphics menu only on a non-repeated F10 keydown");
  menu.draft() = requested;
  check(menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false) ==
                off::ui::GraphicsMenuEffect::closed &&
            menu.confirmed_requested() == baseline,
        "discard an edited draft when F10 closes the menu");

  const auto start = off::ui::GraphicsClock::time_point{};
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  menu.draft() = requested;
  const auto proposal = menu.request_apply();
  check(proposal.has_value() && proposal->requested == requested &&
            proposal->effective.present_mode ==
                off::settings::PresentMode::vsync &&
            proposal->display_confirmation_required &&
            menu.phase() == off::ui::GraphicsMenuPhase::applying,
        "prepare capability-resolved settings without committing the draft");
  check(menu.acknowledge_apply(true, start) ==
                off::ui::GraphicsMenuEffect::none &&
            menu.phase() == off::ui::GraphicsMenuPhase::confirming &&
            menu.confirmed_requested() == baseline &&
            menu.confirmation_deadline() == start + std::chrono::seconds{15},
        "start a 15-second confirmation only after runtime apply succeeds");
  check(menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false) ==
                off::ui::GraphicsMenuEffect::none &&
            menu.tick(start + std::chrono::seconds{14}) ==
                off::ui::GraphicsMenuEffect::none &&
            menu.tick(start + std::chrono::seconds{15}) ==
                off::ui::GraphicsMenuEffect::revert_requested &&
            menu.acknowledge_revert(true) ==
                off::ui::GraphicsMenuEffect::closed &&
            menu.confirmed_requested() == baseline,
        "keep F10 from hiding confirmation and roll back exactly at timeout");

  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  menu.draft() = requested;
  static_cast<void>(menu.request_apply());
  static_cast<void>(menu.acknowledge_apply(true, start));
  check(menu.confirm() == off::ui::GraphicsMenuEffect::commit_requested &&
            menu.confirmed_requested() == requested &&
            menu.phase() == off::ui::GraphicsMenuPhase::closed,
        "commit requested and effective settings only after confirmation");

  check(menu.handle_key(off::ui::GraphicsMenuKey::escape, true, false) ==
            off::ui::GraphicsMenuEffect::quit_requested,
        "retain Escape-to-quit only while the graphics menu is closed");

  return failures == 0 ? 0 : 1;
}
