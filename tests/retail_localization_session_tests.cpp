#include "off/ui/retail_localization_session.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void append_u64(std::string &output, std::uint64_t value) {
  for (unsigned byte{}; byte < 8U; ++byte)
    output.push_back(static_cast<char>((value >> (byte * 8U)) & 0xffU));
}
void append_blob(std::string &output, std::string_view value) {
  append_u64(output, value.size());
  output.append(value);
}
std::string pack(std::string_view parser, std::string_view source_set,
                 std::string_view locale, std::string_view id,
                 std::string_view translation) {
  std::string output{"OFF-PRIVATE-L10N\0\1", 18};
  append_blob(output, parser);
  append_blob(output, source_set);
  append_blob(output, locale);
  append_u64(output, 0U);
  append_u64(output, 2U);
  output.push_back('\0');
  append_u64(output, 1U);
  append_blob(output, id);
  append_blob(output, translation);
  return output;
}
void write(const std::filesystem::path &path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  check(static_cast<bool>(output), "authored fixture writes");
}
} // namespace

int main() {
  using namespace off::ui::l10n;
  try {
    const auto root = std::filesystem::path{OFF_TEST_WORK_DIR};
    const auto cache = root / "cache";
    const auto packs = root / "packs";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(packs, error);
    check(!error, "fixture directories exist");

    constexpr std::string_view installation{"install.fixture.v1"};
    constexpr std::string_view parser{"parser.fixture.v1"};
    constexpr std::string_view source_set{"source.fixture.v1"};
    const auto opaque_one = make_retail_string_id(source_set, 1U);
    const TranslationSourceBinding reviewed_binding{std::string{parser},
                                                    std::string{source_set}, 0U, 2U};
    const auto reviewed = ReviewedRetailLookupArtifact::reviewed(
        reviewed_binding, {{"site.fixture.status", 1U}, {"site.fixture.partial", 0U}});
    check(reviewed.has_value(), "authored source-free observation is reviewable");
    write(packs / "sv.offl10n",
          pack(parser, source_set, "sv", opaque_one, "Projektöversättning"));

    unsigned calls{};
    const auto extract = [&]() -> std::optional<std::vector<RetailSourceString>> {
      ++calls;
      return std::vector<RetailSourceString>{{0U, "fixture source zero"},
                                              {1U, "fixture source one"}};
    };
    const auto first = RetailLocalizationSession::open(
        cache, packs, installation, parser, source_set, extract, {*reviewed});
    check(first && calls == 1U && first->has_local_resolver() &&
              first->metadata().ordinal_count == 2U &&
              first->cache_status() == RetailLocalizationCacheStatus::extracted &&
              first->local_pack_count() == 1U && first->has_private_fallback(),
          "session owns private enrollment and bounded local packs");
    const std::string_view locales[] = {"sv-SE"};
    check(first->resolve_lookup_site("site.fixture.status", {}, locales) ==
              "Projektöversättning",
          "session resolves an admitted site using locale fallback");
    check(first->resolve_lookup_site("site.fixture.partial",
                                   "sv", locales) == "fixture source zero",
          "partial authored packs fall back to the matching private cache");
    check(!first->resolve_lookup_site("site.fixture.unobserved", "sv", locales),
          "session rejects unobserved lookup sites");

    const auto second = RetailLocalizationSession::open(
        cache, packs, installation, parser, source_set, extract, {*reviewed});
    check(second && calls == 1U &&
              second->cache_status() == RetailLocalizationCacheStatus::loaded &&
              second->local_pack_count() == 1U && second->has_private_fallback(),
          "matching session reuses private cache without source extraction");

    const auto no_packs = RetailLocalizationSession::open(
        cache, root / "missing-packs", installation, parser, source_set,
        extract, {*reviewed});
    check(no_packs && !no_packs->has_local_resolver() &&
              no_packs->local_pack_count() == 0U && no_packs->has_private_fallback() &&
              no_packs->resolve_lookup_site("site.fixture.status", "sv", locales) ==
                  "fixture source one",
          "missing optional packs retain the private fallback only");

    const auto malformed = ReviewedRetailLookupArtifact::reviewed(
        reviewed_binding, {{"invalid site", 1U}});
    check(!malformed, "artifact rejects malformed opaque site labels");
    const auto out_of_range = ReviewedRetailLookupArtifact::reviewed(
        reviewed_binding, {{"site.fixture.bad", 2U}});
    check(out_of_range && !RetailLocalizationSession::open(
        cache, packs, installation, parser, source_set, extract, {*out_of_range}),
        "session rejects observed ordinals outside its enrolled span");
    const auto duplicate = ReviewedRetailLookupArtifact::reviewed(
        reviewed_binding, {{"site.fixture.status", 1U}, {"site.fixture.status", 1U}});
    check(duplicate && !RetailLocalizationSession::open(
        cache, packs, installation, parser, source_set, extract, {*duplicate}),
        "session rejects duplicate reviewed sites");
    auto wrong_binding = reviewed_binding;
    wrong_binding.source_set = "source.fixture.other";
    const auto mismatched = ReviewedRetailLookupArtifact::reviewed(
        wrong_binding, {{"site.fixture.other", 1U}});
    check(mismatched && !RetailLocalizationSession::open(
        cache, packs, installation, parser, source_set, extract, {*mismatched}),
        "session rejects an artifact for another enrolled source binding");

    std::filesystem::remove_all(root, error);
    std::cout << "retail localization session tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
