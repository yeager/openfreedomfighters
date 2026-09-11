#include "off/ui/retail_localization_cache.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
}

int main() {
  using namespace off::ui::l10n;
  try {
    const auto root = std::filesystem::current_path() / "off-retail-l10n-cache-tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    unsigned extraction_calls{};
    const auto extract = [&]() -> std::optional<std::vector<RetailSourceString>> {
      ++extraction_calls;
      return std::vector<RetailSourceString>{{0U, "Project-authored source one"}, {1U, "Project-authored source two"}};
    };
    const auto first = ensure_retail_localization_snapshot(root, "install-test-v1", "loc-parser-v1", "ff.loc.startup.v1", extract);
    check(first.status == RetailLocalizationCacheStatus::extracted && first.snapshot && extraction_calls == 1U,
          "first supported extraction publishes a private snapshot");
    const auto second = ensure_retail_localization_snapshot(root, "install-test-v1", "loc-parser-v1", "ff.loc.startup.v1", extract);
    check(second.status == RetailLocalizationCacheStatus::loaded && second.snapshot && extraction_calls == 1U,
          "matching private snapshot does not re-extract source text");
    check(make_retail_string_id("ff.loc.startup.v1", 9U) == "off.retail.ff.loc.startup.v1.9",
          "opaque ID is stable and contains no source text");
    const auto translations = RetailTranslationCatalog::build({
        {"off.retail.ff.loc.startup.v1.0", "Independently authored translation"},
        {"off.retail.ff.loc.startup.v1.1", "Another independently authored translation"}});
    check(translations && translations->find("off.retail.ff.loc.startup.v1.1") &&
              !translations->find("off.retail.ff.loc.startup.v1.2"),
          "translation catalog resolves IDs without retaining source text");
    const auto invalid_utf8 = RetailTranslationCatalog::build({
        {"off.retail.ff.loc.startup.v1.0", std::string{"bad\xc3", 4}}});
    check(!invalid_utf8, "translation catalog rejects invalid UTF-8 rather than guessing an encoding");
    unsigned alternate_extractions{};
    const auto alternate = ensure_retail_localization_snapshot(
        root, "install-test-v1", "loc-parser-v1", "ff.loc.menu.v1", [&] {
          ++alternate_extractions;
          return std::optional<std::vector<RetailSourceString>>{
              std::vector<RetailSourceString>{{0U, "Project-authored alternate source"}}};
        });
    check(alternate.status == RetailLocalizationCacheStatus::extracted && alternate_extractions == 1U,
          "different source-set identity never reuses an incompatible source catalog");
    const auto invalid = ensure_retail_localization_snapshot(root, "install-test-v2", "loc-parser-v1", "ff.loc.startup.v1", [] {
      return std::optional<std::vector<RetailSourceString>>{std::vector<RetailSourceString>{{1U, "wrong ordinal"}}};
    });
    check(invalid.status == RetailLocalizationCacheStatus::invalid,
          "out-of-order extraction cannot publish a partial catalog");
    std::filesystem::remove_all(root, error);
    std::cout << "retail localization cache tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
