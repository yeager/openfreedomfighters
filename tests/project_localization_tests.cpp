#include "off/ui/project_localization.hpp"
#include "off/platform/locale_preferences.hpp"
#include "off/platform/startup_data_error_presentation.hpp"

#include <array>
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
  using off::platform::canonical_host_locale_preferences;
  using off::platform::canonical_host_locale_tag;
  check(canonical_host_locale_tag("sv_SE.UTF-8") == "sv-SE",
        "POSIX host spelling is canonicalized before locale selection");
  check(canonical_host_locale_tag("zh_hans_cn") == "zh-Hans-CN",
        "script and region casing are canonicalized deterministically");
  check(!canonical_host_locale_tag("sv--SE") &&
            !canonical_host_locale_tag("sv\x01SE") &&
            !canonical_host_locale_tag("C"),
        "malformed and non-language host values are rejected");
  constexpr std::array<std::string_view, 5> host_values{{
      "sv_SE.UTF-8", "SV-se", "bad--tag", "en_US", "sv-SE"}};
  const auto host_preferences = canonical_host_locale_preferences(host_values);
  check(host_preferences.size() == 2U && host_preferences[0] == "sv-SE" &&
            host_preferences[1] == "en-US",
        "host preference order is preserved after malformed values and duplicates are removed");
  using namespace off::ui::l10n;
  const auto &catalog = f10_catalog();
  check(catalog.resolve(MessageId::apply, "sv-SE", "en-US") == "Tillämpa",
        "explicit supported locale wins");
  check(catalog.resolve(MessageId::apply, "xx-XX", "sv-SE") == "Tillämpa",
        "platform locale follows unavailable explicit locale");
  constexpr std::array<std::string_view, 2> ordered_platform_locales{{"xx-XX", "sv-SE"}};
  check(catalog.resolve(MessageId::apply, "", ordered_platform_locales) == "Tillämpa",
        "supported secondary system locale follows unsupported primary locale");
  check(catalog.resolve(MessageId::apply, "de-DE", ordered_platform_locales) == "Anwenden",
        "explicit locale remains ahead of all system preferences");
  check(catalog.resolve(MessageId::apply, "xx-XX", "zz-ZZ") == "Apply",
        "English is the deterministic final fallback");
  check(catalog.resolve(MessageId::revert, "SV", "en") == "Återställ",
        "locale matching accepts language case");
  check(catalog.resolve(MessageId::back, "", "sv_SE.UTF-8") == "Tillbaka",
        "common platform locale spellings resolve by language");
  check(catalog.format_seconds(MessageId::reverting_in_seconds, 12, "sv",
                               "en") == "Återställer om 12 sekunder",
        "UTF-8 catalog text survives formatted output");
  check(catalog.resolve(MessageId::verifying_game_data, "sv-SE", "en-US") ==
            "Verifierar speldata...",
        "startup verification status follows the selected locale");
  check(catalog.resolve(MessageId::preparing_startup, "zh-CN", "en-US") ==
            "正在准备启动...",
        "startup preparation status resolves as UTF-8");
  constexpr std::array<std::string_view, locale_count> locale_tags{{
      "en-US", "sv-SE", "da-DK", "nb-NO", "fi-FI", "de-DE", "fr-FR",
      "es-ES", "it-IT", "pt-BR", "pl-PL", "cs-CZ", "hu-HU", "ro-RO",
      "tr-TR", "ru-RU", "uk-UA", "ja-JP", "ko-KR", "zh-CN",
  }};
  for (const auto tag : locale_tags)
    for (std::size_t id = 0; id < message_id_count; ++id)
      check(catalog.resolve(static_cast<MessageId>(id), tag, "en-US")
                .has_value(),
            "every supported F10 catalog contains every stable message ID");
  check(catalog.resolve(MessageId::apply, "ru-RU", "en-US") == "Применить",
        "Cyrillic F10 text resolves from the Russian catalog");
  check(catalog.resolve(MessageId::graphics_settings, "ja-JP", "en-US") ==
            "グラフィック設定",
        "Japanese F10 text resolves as UTF-8");
  check(catalog.resolve(MessageId::graphics_settings, "zh-CN", "en-US") ==
            "图形设置",
        "Simplified Chinese F10 text resolves as UTF-8");
  check(catalog.resolve(MessageId::apply, "xx-XX", "ko-KR") == "적용",
        "a supported platform locale follows an unavailable explicit locale");
  check(catalog.resolve(MessageId::apply, "zh-Hant", "en-US") == "Apply",
        "a Traditional Chinese request does not select the Simplified catalog");
  check(!catalog.resolve(static_cast<MessageId>(message_id_count), "en", "en"),
        "invalid message IDs do not resolve");
  check(!catalog.format_seconds(MessageId::apply, 2, "en", "en"),
        "only the declared countdown pattern is formatted");
  const auto pseudo_apply = pseudo_localized_f10_text(MessageId::apply);
  check(pseudo_apply && *pseudo_apply == "[!! ÀÀpply !!]",
        "pseudo-localization is deterministic for an authored F10 ID");
  const auto pseudo_countdown =
      pseudo_localized_f10_text(MessageId::reverting_in_seconds);
  check(pseudo_countdown &&
            pseudo_countdown->find("{seconds}") != std::string::npos,
        "pseudo-localization retains the declared formatting marker");
  check(!pseudo_localized_f10_text(static_cast<MessageId>(message_id_count)),
        "pseudo-localization rejects IDs outside the authored F10 catalog");

  const off::data::InstallVerification missing_executable{
      .error = off::data::InstallError::missing_executable,
      .message = "exact verifier diagnostic"};
  const auto startup_error = off::platform::make_startup_data_error_presentation(
      missing_executable, catalog, "sv-SE", "en-US");
  check(startup_error.title == "Speldata krävs",
        "startup error title follows the selected locale");
  check(startup_error.summary ==
            "Freedom.Exe saknas i den valda speldata-mappen.",
        "stable install error selects a localized explanation");
  check(startup_error.support_code == "OFF-DATA-02",
        "stable install error selects a path-free support code");
  check(startup_error.dialog_text().find("exact verifier diagnostic") ==
            std::string::npos,
        "popup never exposes a raw verifier diagnostic");
  check(startup_error.dialog_text().find("OFF-DATA-02") != std::string::npos,
        "dialog composition retains its support code");
  check(off::platform::startup_data_error_message_id(
            off::data::InstallError::unsupported_executable_hash) ==
            MessageId::game_executable_unsupported,
        "executable hash failure has a stable presentation mapping");
  check(off::platform::startup_data_error_message_id(
            off::data::InstallError::none) ==
            MessageId::game_data_verification_failed,
        "unexpected verification result has a safe presentation mapping");
  check(off::platform::startup_data_error_support_code(
            off::data::InstallError::missing_root) == "OFF-DATA-01" &&
            off::platform::startup_data_error_support_code(
                off::data::InstallError::unsupported_executable_size) ==
                "OFF-DATA-03" &&
            off::platform::startup_data_error_support_code(
                off::data::InstallError::unsupported_executable_hash) ==
                "OFF-DATA-03" &&
            off::platform::startup_data_error_support_code(
                off::data::InstallError::incomplete_game_data) ==
                "OFF-DATA-04" &&
            off::platform::startup_data_error_support_code(
                off::data::InstallError::io_error) == "OFF-DATA-05",
        "all verifier states have a stable path-free support code");

  const std::array<CatalogEntry, 1> incomplete{{
      {Locale::english, MessageId::apply, "Apply"},
  }};
  const auto missing = ProjectCatalog::build(incomplete);
  check(!missing.catalog && missing.error == CatalogError::missing_message,
        "incomplete catalogs are rejected");
  const std::array<CatalogEntry, 1> malformed{{
      {Locale::english, MessageId::apply, std::string_view{"\xc3\x28", 2}},
  }};
  const auto invalid_utf8 = ProjectCatalog::build(malformed);
  check(!invalid_utf8.catalog &&
            invalid_utf8.error == CatalogError::invalid_utf8,
        "malformed UTF-8 is rejected before any lookup");
  const std::array<CatalogEntry, 1> invalid_id{{
      {Locale::english, static_cast<MessageId>(message_id_count), "X"},
  }};
  const auto invalid_message = ProjectCatalog::build(invalid_id);
  check(!invalid_message.catalog &&
            invalid_message.error == CatalogError::invalid_message_id,
        "out-of-range message IDs are rejected");
  const std::array<CatalogEntry, 2> duplicate{{
      {Locale::english, MessageId::apply, "Apply"},
      {Locale::english, MessageId::apply, "Again"},
  }};
  const auto duplicate_message = ProjectCatalog::build(duplicate);
  check(!duplicate_message.catalog &&
            duplicate_message.error == CatalogError::duplicate_message,
        "duplicate message IDs are rejected");
  return failures == 0 ? 0 : 1;
}
