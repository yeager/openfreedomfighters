#include "off/settings/graphics_settings.hpp"
#include "off/ui/graphics_menu.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>

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

  unsigned initial_apply_calls = 0;
  std::optional<off::settings::EffectiveGraphicsSettings> initial_applied;
  check(off::settings::initialize_graphics_settings(
            resolution, [&](const off::settings::EffectiveGraphicsSettings &value) {
              ++initial_apply_calls;
              initial_applied = value;
              return true;
            }) == off::settings::InitialGraphicsSetup::ready &&
            initial_apply_calls == 1 && initial_applied == resolution.effective,
        "apply the resolved boot configuration exactly once before rendering");
  initial_apply_calls = 0;
  check(off::settings::initialize_graphics_settings(
            resolution, [&](const off::settings::EffectiveGraphicsSettings &) {
              ++initial_apply_calls;
              return false;
            }) == off::settings::InitialGraphicsSetup::apply_failed &&
            initial_apply_calls == 1,
        "surface a boot display-configuration failure after one apply attempt");

  std::vector<off::settings::EffectiveGraphicsSettings> transaction_attempts;
  auto alternative_effective = *resolution.effective;
  alternative_effective.present_mode = off::settings::PresentMode::mailbox;
  const auto successful_transaction =
      off::settings::apply_graphics_transaction(
          *resolution.effective, alternative_effective, [&](const auto &value) {
            transaction_attempts.push_back(value);
            return true;
          });
  check(successful_transaction == off::settings::GraphicsApplyTransaction::applied &&
            transaction_attempts ==
                std::vector<off::settings::EffectiveGraphicsSettings>{alternative_effective},
        "a successful display apply does not perform an unnecessary rollback");
  transaction_attempts.clear();
  const auto transaction = off::settings::apply_graphics_transaction(
      *resolution.effective, alternative_effective, [&](const auto &value) {
        transaction_attempts.push_back(value);
        return transaction_attempts.size() == 2;
      });
  check(transaction == off::settings::GraphicsApplyTransaction::restored_previous &&
            transaction_attempts == std::vector<off::settings::EffectiveGraphicsSettings>{
                                        alternative_effective, *resolution.effective},
        "a failed display apply restores the complete prior effective state");
  transaction_attempts.clear();
  const auto unrecoverable_transaction =
      off::settings::apply_graphics_transaction(
          *resolution.effective, alternative_effective, [&](const auto &value) {
            transaction_attempts.push_back(value);
            return false;
          });
  check(unrecoverable_transaction ==
                off::settings::GraphicsApplyTransaction::restore_failed &&
            transaction_attempts == std::vector<off::settings::EffectiveGraphicsSettings>{
                                        alternative_effective, *resolution.effective},
        "a failed rollback remains visible to the runtime instead of fabricating recovery");

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

  auto portable_extent = capabilities;
  portable_extent.maximum_windowed_size = {1280, 720};
  const auto boot_recovery = off::settings::resolve_initial_graphics_settings(
      requested, portable_extent);
  check(boot_recovery.recovered_windowed_size &&
            boot_recovery.requested.windowed_size ==
                off::settings::WindowSize{1280, 720} &&
            boot_recovery.resolution.effective.has_value() &&
            boot_recovery.resolution.effective->windowed_size ==
                off::settings::WindowSize{1280, 720},
        "clamp a portable startup extent to the current native display bounds");
  const auto interactive_extent =
      off::settings::resolve_graphics_settings(requested, portable_extent);
  check(!interactive_extent.effective &&
            interactive_extent.error ==
                off::settings::GraphicsValidationError::window_size_above_maximum,
        "keep interactive out-of-range requests explicit instead of silently clamping them");
  auto impossible_extent = portable_extent;
  impossible_extent.minimum_windowed_size = {1920, 1080};
  const auto rejected_boot_recovery =
      off::settings::resolve_initial_graphics_settings(requested, impossible_extent);
  check(!rejected_boot_recovery.recovered_windowed_size &&
            !rejected_boot_recovery.resolution.effective &&
            rejected_boot_recovery.resolution.error ==
                off::settings::GraphicsValidationError::window_size_above_maximum,
        "refuse to invent a startup extent for contradictory backend bounds");
  auto handheld_defaults = capabilities;
  handheld_defaults.original_profile = false;
  handheld_defaults.minimum_windowed_size = {1920, 1080};
  handheld_defaults.maximum_windowed_size = {1920, 1080};
  const auto safe_handheld_defaults =
      off::settings::make_safe_default_graphics_settings(handheld_defaults);
  const auto resolved_safe_handheld_defaults = safe_handheld_defaults
      ? off::settings::resolve_graphics_settings(*safe_handheld_defaults,
                                                  handheld_defaults)
      : off::settings::GraphicsResolution{};
  check(safe_handheld_defaults &&
            safe_handheld_defaults->profile == off::Mode::modern &&
            safe_handheld_defaults->windowed_size ==
                off::settings::WindowSize{1920, 1080} &&
            safe_handheld_defaults->upscaler == off::settings::Upscaler::native &&
            resolved_safe_handheld_defaults.effective.has_value(),
        "derive an immediately applicable portable Defaults preset without "
        "enabling vendor upscalers");
  auto incoherent_defaults = handheld_defaults;
  incoherent_defaults.minimum_windowed_size = {2560, 1440};
  check(!off::settings::make_safe_default_graphics_settings(incoherent_defaults),
        "refuse a Defaults preset for contradictory native bounds");
  initial_apply_calls = 0;
  check(off::settings::initialize_graphics_settings(
            invalid_resolution,
            [&](const off::settings::EffectiveGraphicsSettings &) {
              ++initial_apply_calls;
              return true;
            }) == off::settings::InitialGraphicsSetup::invalid_resolution &&
            initial_apply_calls == 0,
        "do not call a display backend for an invalid boot configuration");
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
                off::settings::FallbackReason::modern_plus_upscaler_required &&
            advanced_fallback.effective->fallbacks[2].reason ==
                off::settings::FallbackReason::mailbox_unavailable,
        "preserve Modern+ intent while resolving portable advanced fallbacks");

  advanced.modern_plus = true;
  advanced.upscaler = off::settings::Upscaler::xess;
  const auto xess_fallback =
      off::settings::resolve_graphics_settings(advanced, portable);
  check(xess_fallback.effective.has_value() &&
            xess_fallback.effective->upscaler ==
                off::settings::Upscaler::temporal &&
            xess_fallback.effective->fallbacks.size() == 3 &&
            xess_fallback.effective->fallbacks[1].reason ==
                off::settings::FallbackReason::modern_plus_upscaler_required,
        "fall back from a rejected Modern+ XeSS request to portable temporal upscaling");

  advanced.upscaler = off::settings::Upscaler::fsr;
  const auto fsr_fallback =
      off::settings::resolve_graphics_settings(advanced, portable);
  check(fsr_fallback.effective.has_value() &&
            fsr_fallback.effective->upscaler ==
                off::settings::Upscaler::temporal &&
            fsr_fallback.effective->fallbacks.size() == 3 &&
            fsr_fallback.effective->fallbacks[1].reason ==
                off::settings::FallbackReason::modern_plus_upscaler_required,
        "fall back from a rejected Modern+ FSR request to portable temporal upscaling");

  auto fsr_available = portable;
  fsr_available.modern_plus = true;
  fsr_available.fsr_upscaler = true;
  const auto fsr_enabled =
      off::settings::resolve_graphics_settings(advanced, fsr_available);
  check(fsr_enabled.effective.has_value() &&
            fsr_enabled.effective->upscaler == off::settings::Upscaler::fsr &&
            fsr_enabled.effective->fallbacks.size() == 1,
        "preserve an FSR request only when the runtime reports a loaded adapter");

  // Capability objects can arrive from a platform bridge.  Reject a
  // contradictory bridge result instead of interpreting its provider bit as
  // permission to bypass the Modern+ admission boundary.
  auto contradictory_fsr = fsr_available;
  contradictory_fsr.modern_plus = false;
  const auto contradictory_fallback =
      off::settings::resolve_graphics_settings(advanced, contradictory_fsr);
  check(contradictory_fallback.effective.has_value() &&
            !contradictory_fallback.effective->modern_plus &&
            contradictory_fallback.effective->upscaler ==
                off::settings::Upscaler::temporal &&
            contradictory_fallback.effective->fallbacks.size() == 3 &&
            contradictory_fallback.effective->fallbacks[0].reason ==
                off::settings::FallbackReason::modern_plus_unavailable &&
            contradictory_fallback.effective->fallbacks[1].reason ==
                off::settings::FallbackReason::modern_plus_upscaler_required,
        "a provider claim cannot bypass rejected Modern+ admission");

  // A capability object may cross process boundaries in a future native
  // backend.  Even a complete vendor claim must not turn a plain Modern
  // request into a Modern+ renderer path.
  auto vendor_ready = capabilities;
  vendor_ready.temporal_upscaler = true;
  vendor_ready.dlss_upscaler = true;
  vendor_ready.fsr_upscaler = true;
  vendor_ready.xess_upscaler = true;
  advanced.modern_plus = false;
  advanced.upscaler = off::settings::Upscaler::dlss;
  const auto plain_modern_vendor =
      off::settings::resolve_graphics_settings(advanced, vendor_ready);
  check(plain_modern_vendor.effective.has_value() &&
            !plain_modern_vendor.effective->modern_plus &&
            plain_modern_vendor.effective->upscaler ==
                off::settings::Upscaler::temporal &&
            plain_modern_vendor.effective->fallbacks.size() == 2 &&
            plain_modern_vendor.effective->fallbacks[0].reason ==
                off::settings::FallbackReason::modern_plus_upscaler_required &&
            plain_modern_vendor.effective->fallbacks[1].reason ==
                off::settings::FallbackReason::mailbox_unavailable,
        "never activate a vendor upscaler outside Modern+ even when a "
        "capability object claims it is ready");

  advanced.modern_plus = true;
  advanced.upscaler = off::settings::Upscaler::dlss;
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
  check(menu.select_row(off::ui::GraphicsMenuRow::upscaler),
        "allow the active F10 session to focus the upscaler row");
  menu.draft().upscaler = off::settings::Upscaler::native;
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().upscaler == off::settings::Upscaler::native,
        "Original never exposes an unavailable upscaler choice");
  menu.draft().profile = off::Mode::modern;
  menu.draft().modern_plus = false;
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().upscaler == off::settings::Upscaler::temporal,
        "plain Modern exposes its available temporal provider");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().upscaler == off::settings::Upscaler::native,
        "F10 skips unavailable vendor provider names instead of accepting a "
        "request that would immediately fall back");
  check(menu.select_row(off::ui::GraphicsMenuRow::present_mode),
        "focus Present Mode for availability filtering");
  menu.draft().present_mode = off::settings::PresentMode::vsync;
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::right, true, false));
  check(menu.draft().present_mode == off::settings::PresentMode::vsync,
        "F10 skips unsupported present modes rather than displaying them");
  check(menu.select_row(off::ui::GraphicsMenuRow::profile),
        "return focus to Profile before testing top-of-menu wrapping");
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::up, true, false));
  check(menu.selected_row() == off::ui::GraphicsMenuRow::defaults,
        "wrap upward from the first row to Defaults");
  menu.draft() = requested;
  menu.draft().modern_plus = true;
  menu.draft().render_scale_percent = 200;
  menu.draft().upscaler = off::settings::Upscaler::dlss;
  menu.draft().shadow_quality = off::settings::ShadowQuality::ultra;
  static_cast<void>(
      menu.handle_key(off::ui::GraphicsMenuKey::enter, true, false));
  check(menu.draft() == *off::settings::make_safe_default_graphics_settings(capabilities) &&
            menu.confirmed_requested() == baseline &&
            menu.phase() == off::ui::GraphicsMenuPhase::editing,
        "Defaults resets only the draft to a safe preset and waits for Apply");
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

  off::ui::GraphicsMenuSession restored_menu{capabilities};
  const auto stored_advanced = off::settings::RequestedGraphicsSettings{
      .profile = off::Mode::modern,
      .window_mode = off::settings::WindowMode::windowed,
      .windowed_size = {1280, 720},
      .present_mode = off::settings::PresentMode::mailbox,
      .modern_plus = true,
      .render_scale_percent = 100,
      .upscaler = off::settings::Upscaler::dlss,
      .shadow_quality = off::settings::ShadowQuality::high,
  };
  const auto restored =
      off::settings::resolve_graphics_settings(stored_advanced, capabilities);
  check(restored.effective.has_value(), "resolve stale stored graphics intent");
  if (restored.effective) {
    restored_menu.set_confirmed(stored_advanced, *restored.effective);
    static_cast<void>(restored_menu.handle_key(off::ui::GraphicsMenuKey::f10,
                                                true, false));
    check(restored_menu.draft().present_mode ==
                  restored.effective->present_mode &&
              restored_menu.draft().upscaler == restored.effective->upscaler &&
              restored_menu.draft().modern_plus == restored.effective->modern_plus,
          "F10 opens stale stored preferences at their active native values");
  }

  auto provider_capabilities = capabilities;
  provider_capabilities.dlss_upscaler = true;
  provider_capabilities.fsr_upscaler = true;
  provider_capabilities.xess_upscaler = true;
  off::ui::GraphicsMenuSession provider_menu{provider_capabilities};
  static_cast<void>(
      provider_menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  provider_menu.draft().profile = off::Mode::modern;
  provider_menu.draft().modern_plus = true;
  provider_menu.draft().upscaler = off::settings::Upscaler::native;
  check(provider_menu.select_row(off::ui::GraphicsMenuRow::upscaler),
        "focus the provider selector with complete runtime capabilities");
  for (int step = 0; step < 3; ++step)
    static_cast<void>(provider_menu.handle_key(
        off::ui::GraphicsMenuKey::right, true, false));
  check(provider_menu.draft().upscaler == off::settings::Upscaler::fsr,
        "Modern Plus exposes every negotiated provider in selector order");

  off::ui::GraphicsMenuSession handheld_menu{handheld_defaults};
  static_cast<void>(
      handheld_menu.handle_key(off::ui::GraphicsMenuKey::f10, true, false));
  handheld_menu.draft() = requested;
  check(handheld_menu.select_row(off::ui::GraphicsMenuRow::defaults),
        "focus Defaults on a constrained native display");
  static_cast<void>(
      handheld_menu.handle_key(off::ui::GraphicsMenuKey::enter, true, false));
  const auto handheld_proposal = handheld_menu.request_apply();
  check(handheld_proposal &&
            handheld_proposal->requested.windowed_size ==
                off::settings::WindowSize{1920, 1080} &&
            handheld_proposal->effective.upscaler ==
                off::settings::Upscaler::native,
        "F10 Defaults recovers a constrained display with an applicable "
        "native-only preset");

  return failures == 0 ? 0 : 1;
}
