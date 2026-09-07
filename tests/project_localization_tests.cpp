#include "off/ui/project_localization.hpp"

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
  using namespace off::ui::l10n;
  const auto &catalog = f10_catalog();
  check(catalog.resolve(MessageId::apply, "sv-SE", "en-US") == "Tillämpa",
        "explicit supported locale wins");
  check(catalog.resolve(MessageId::apply, "fi-FI", "sv-SE") == "Tillämpa",
        "platform locale follows unavailable explicit locale");
  check(catalog.resolve(MessageId::apply, "fi-FI", "ja-JP") == "Apply",
        "English is the deterministic final fallback");
  check(catalog.resolve(MessageId::revert, "SV", "en") == "Återställ",
        "locale matching accepts language case");
  check(catalog.resolve(MessageId::back, "", "sv_SE.UTF-8") == "Tillbaka",
        "common platform locale spellings resolve by language");
  check(catalog.format_seconds(MessageId::reverting_in_seconds, 12, "sv",
                               "en") == "Återställer om 12 sekunder",
        "UTF-8 catalog text survives formatted output");
  check(!catalog.resolve(static_cast<MessageId>(message_id_count), "en", "en"),
        "invalid message IDs do not resolve");
  check(!catalog.format_seconds(MessageId::apply, 2, "en", "en"),
        "only the declared countdown pattern is formatted");

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
