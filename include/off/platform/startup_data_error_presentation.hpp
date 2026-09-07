#pragma once

#include "off/data/install.hpp"
#include "off/ui/project_localization.hpp"

#include <string>
#include <string_view>

namespace off::platform {

// This is deliberately independent of SDL and of the verifier. The verifier's
// stable error enum selects the user-facing explanation, while its raw message
// is retained separately for support and terminal diagnostics.
struct StartupDataErrorPresentation {
  std::string title;
  std::string summary;
  std::string relaunch_hint;
  std::string technical_details_label;
  std::string technical_message;

  [[nodiscard]] std::string dialog_text() const {
    std::string result = summary + "\n\n" + relaunch_hint;
    if (!technical_message.empty())
      result += "\n\n" + technical_details_label + ": " + technical_message;
    return result;
  }
};

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

[[nodiscard]] inline StartupDataErrorPresentation
make_startup_data_error_presentation(
    const data::InstallVerification &verification,
    const ui::l10n::ProjectCatalog &catalog, std::string_view explicit_locale,
    std::string_view platform_locale) {
  const auto text = [&](ui::l10n::MessageId id) {
    const auto resolved = catalog.resolve(id, explicit_locale, platform_locale);
    return std::string{resolved.value_or("OpenFreedomFighters")};
  };
  return {.title = text(ui::l10n::MessageId::game_data_required),
          .summary = text(startup_data_error_message_id(verification.error)),
          .relaunch_hint = text(ui::l10n::MessageId::relaunch_with_data_path),
          .technical_details_label =
              text(ui::l10n::MessageId::technical_details),
          .technical_message = verification.message};
}

} // namespace off::platform
