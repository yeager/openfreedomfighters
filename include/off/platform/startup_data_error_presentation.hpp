#pragma once

#include "off/data/install.hpp"
#include "off/ui/project_localization.hpp"

#include <array>
#include <string>
#include <string_view>
#include <span>

namespace off::platform {

// This is deliberately independent of SDL and of the verifier. The verifier's
// stable error enum selects both the user-facing explanation and a support
// code.  A verifier diagnostic is intentionally not copied into the popup:
// filesystem and parser diagnostics can contain a machine-local path or a
// retail resource name.  Callers that need it for a terminal diagnostic retain
// InstallVerification separately.
struct StartupDataErrorPresentation {
  std::string title;
  std::string summary;
  std::string relaunch_hint;
  std::string technical_details_label;
  std::string support_code;

  [[nodiscard]] std::string dialog_text() const {
    std::string result = summary + "\n\n" + relaunch_hint;
    if (!support_code.empty())
      result += "\n\n" + technical_details_label + ": " + support_code;
    return result;
  }
};

[[nodiscard]] inline StartupDataErrorPresentation
make_startup_data_error_presentation(
    const data::InstallVerification &verification,
    const ui::l10n::ProjectCatalog &catalog, std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales);

[[nodiscard]] inline ui::l10n::MessageId
startup_data_error_message_id(data::InstallError error) noexcept {
  using data::InstallError;
  using ui::l10n::MessageId;
  switch (error) {
  case InstallError::missing_root:
    return MessageId::game_data_folder_unavailable;
  case InstallError::missing_executable:
    return MessageId::game_executable_missing;
  case InstallError::unsupported_executable_size:
  case InstallError::unsupported_executable_hash:
    return MessageId::game_executable_unsupported;
  case InstallError::incomplete_game_data:
    return MessageId::game_data_incomplete;
  case InstallError::io_error:
    return MessageId::game_data_unreadable;
  case InstallError::none:
    return MessageId::game_data_verification_failed;
  }
  return MessageId::game_data_verification_failed;
}

// Stable, project-authored support identifiers. These do not encode a path,
// manifest member, hash, or other retail-data detail.
[[nodiscard]] inline std::string_view
startup_data_error_support_code(data::InstallError error) noexcept {
  using data::InstallError;
  switch (error) {
  case InstallError::missing_root:
    return "OFF-DATA-01";
  case InstallError::missing_executable:
    return "OFF-DATA-02";
  case InstallError::unsupported_executable_size:
  case InstallError::unsupported_executable_hash:
    return "OFF-DATA-03";
  case InstallError::incomplete_game_data:
    return "OFF-DATA-04";
  case InstallError::io_error:
    return "OFF-DATA-05";
  case InstallError::none:
    return "OFF-DATA-00";
  }
  return "OFF-DATA-00";
}

[[nodiscard]] inline StartupDataErrorPresentation
make_startup_data_error_presentation(
    const data::InstallVerification &verification,
    const ui::l10n::ProjectCatalog &catalog, std::string_view explicit_locale,
    std::string_view platform_locale) {
  const std::array<std::string_view, 1> platform_locales{{platform_locale}};
  return make_startup_data_error_presentation(
      verification, catalog, explicit_locale, std::span{platform_locales});
}

[[nodiscard]] inline StartupDataErrorPresentation
make_startup_data_error_presentation(
    const data::InstallVerification &verification,
    const ui::l10n::ProjectCatalog &catalog, std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales) {
  const auto text = [&](ui::l10n::MessageId id) {
    const auto resolved = catalog.resolve(id, explicit_locale, platform_locales);
    return std::string{resolved.value_or("OpenFreedomFighters")};
  };
  return {.title = text(ui::l10n::MessageId::game_data_required),
          .summary = text(startup_data_error_message_id(verification.error)),
          .relaunch_hint = text(ui::l10n::MessageId::relaunch_with_data_path),
          .technical_details_label =
              text(ui::l10n::MessageId::technical_details),
          .support_code = std::string{startup_data_error_support_code(
              verification.error)}};
}

} // namespace off::platform
