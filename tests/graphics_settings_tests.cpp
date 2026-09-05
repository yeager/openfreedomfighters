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

  auto advanced = requested;
  advanced.modern_plus = true;
  advanced.render_scale_percent = 67;
  advanced.upscaler = off::settings::Upscaler::dlss;
  advanced.shadow_quality = off::settings::ShadowQuality::ultra;
  auto portable = capabilities;
  portable.modern_plus = false;
  portable.dlss_upscaler = false;
  portable.temporal_upscaler = true;
  const auto advanced_fallback =
      off::settings::resolve_graphics_settings(advanced, portable);
  check(advanced_fallback.effective.has_value() &&
            !advanced_fallback.effective->modern_plus &&
            advanced_fallback.effective->render_scale_percent == 67 &&
            advanced_fallback.effective->upscaler ==
                off::settings::Upscaler::temporal &&
            advanced_fallback.effective->shadow_quality ==
                off::settings::ShadowQuality::ultra &&
            advanced_fallback.effective->fallbacks.size() == 3 &&
            advanced_fallback.effective->fallbacks[0].reason ==
                off::settings::FallbackReason::modern_plus_unavailable &&
            advanced_fallback.effective->fallbacks[1].reason ==
                off::settings::FallbackReason::dlss_upscaler_unavailable &&
            advanced_fallback.effective->fallbacks[2].reason ==
                off::settings::FallbackReason::mailbox_unavailable,
        "preserve Modern+ intent while resolving portable advanced fallbacks");

  advanced.profile = off::Mode::original;
  const auto original_advanced =
      off::settings::resolve_graphics_settings(advanced, capabilities);
  check(original_advanced.effective.has_value() &&
            !original_advanced.effective->modern_plus &&
            original_advanced.effective->upscaler ==
                off::settings::Upscaler::native &&
            original_advanced.effective->shadow_quality ==
                off::settings::ShadowQuality::reference &&
            advanced.modern_plus &&
            advanced.upscaler == off::settings::Upscaler::dlss,
        "force reference rendering in Original without overwriting intent");
  advanced.profile = off::Mode::modern;
  advanced.render_scale_percent = 49;
  check(
      off::settings::resolve_graphics_settings(advanced, capabilities).error ==
          off::settings::GraphicsValidationError::render_scale_out_of_range,
      "reject render scales below the supported range");
  advanced.render_scale_percent = 201;
  check(
      off::settings::resolve_graphics_settings(advanced, capabilities).error ==
          off::settings::GraphicsValidationError::render_scale_out_of_range,
      "reject render scales above the supported range");

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

  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::profile,
        "reopen with profile focused");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().profile == off::Mode::modern && !menu.draft().modern_plus,
        "cycle from Original to Modern");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().profile == off::Mode::modern && menu.draft().modern_plus,
        "cycle from Modern to Modern+");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().profile == off::Mode::original &&
            !menu.draft().modern_plus,
        "wrap the profile selector after Modern+");
  static_cast<void>(menu.handle_key(off::ui::GraphicsMenuKey::up, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::defaults,
        "wrap upward from the first row to Defaults");
  menu.draft() = requested;
  menu.draft().modern_plus = true;
  menu.draft().render_scale_percent = 200;
  menu.draft().upscaler = off::settings::Upscaler::dlss;
  menu.draft().shadow_quality = off::settings::ShadowQuality::ultra;
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::enter, true, false));
  check(menu.draft() == off::settings::RequestedGraphicsSettings{} &&
            menu.confirmed_requested() == baseline &&
            menu.phase() == off::ui::GraphicsMenuPhase::editing,
        "Defaults resets only the draft and waits for Apply");
  check(menu.handle_key(off::ui::GraphicsMenuKey::escape, true, false) ==
                off::ui::GraphicsMenuEffect::closed &&
            menu.confirmed_requested() == baseline,
        "cancel a default reset without changing confirmed settings");

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
