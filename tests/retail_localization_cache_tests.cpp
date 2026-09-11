#include "off/ui/retail_localization_cache.hpp"
#include "off/data/loc_catalog.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void append_text(std::vector<std::byte>& output, std::string_view value) {
  for (const auto character : value) output.push_back(static_cast<std::byte>(character));
  output.push_back(std::byte{});
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value) {
  for (unsigned shift{}; shift < 32U; shift += 8U)
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

std::optional<std::vector<off::ui::l10n::RetailSourceString>> synthetic_loc_records() {
  std::vector<std::byte> first;
  append_text(first, "synthetic.first");
  first.push_back(std::byte{1});
  append_text(first, "Project-authored LOC one");
  std::vector<std::byte> second;
  append_text(second, "synthetic.second");
  second.push_back(std::byte{1});
  append_text(second, "Project-authored LOC two");
  std::vector<std::byte> member{std::byte{2}};
  append_u32(member, static_cast<std::uint32_t>(first.size()));
  member.insert(member.end(), first.begin(), first.end());
  member.insert(member.end(), second.begin(), second.end());
  const auto decoded = off::data::decode_loc_member_display_texts(member);
  if (!decoded) return std::nullopt;
  std::vector<off::ui::l10n::RetailSourceString> result;
  result.reserve(decoded->values.size());
  for (std::size_t index{}; index < decoded->values.size(); ++index)
    result.push_back({static_cast<std::uint64_t>(index), decoded->values[index]});
  return result;
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
    const auto blank_source = ensure_retail_localization_snapshot(
        root, "install-test-v1", "loc-parser-v1", "ff.loc.blank.v1", [] {
          return std::optional<std::vector<RetailSourceString>>{
              std::vector<RetailSourceString>{{0U, ""}, {1U, "Project-authored nonblank source"}}};
        });
    check(blank_source.status == RetailLocalizationCacheStatus::extracted && blank_source.snapshot &&
              blank_source.snapshot->strings[0].english.empty() &&
              blank_source.snapshot->strings[1].ordinal == 1U,
          "private source cache preserves intentionally blank records and their ordinals");
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
    unsigned loc_extraction_calls{};
    const auto loc_first = ensure_retail_localization_snapshot(
        root, "install-test-v1", off::data::loc_catalog_parser_identity,
        "loc-source-synthetic-v1", [&] {
          ++loc_extraction_calls;
          return synthetic_loc_records();
        });
    check(loc_first.status == RetailLocalizationCacheStatus::extracted && loc_first.snapshot &&
              loc_first.snapshot->strings.size() == 2U && loc_extraction_calls == 1U,
          "synthetic LOC records feed the local-only cache in decoder order");
    const auto loc_second = ensure_retail_localization_snapshot(
        root, "install-test-v1", off::data::loc_catalog_parser_identity,
        "loc-source-synthetic-v1", [&] {
          ++loc_extraction_calls;
          return synthetic_loc_records();
        });
    check(loc_second.status == RetailLocalizationCacheStatus::loaded && loc_second.snapshot &&
              *loc_first.snapshot == *loc_second.snapshot && loc_extraction_calls == 1U,
          "matching LOC source-set identity reuses the private snapshot without decoding again");
    std::filesystem::remove_all(root, error);
    std::cout << "retail localization cache tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
