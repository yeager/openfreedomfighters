#include "off/ui/private_translation_pack.hpp"

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
struct Entry {
  std::string id;
  std::string text;
};
std::string pack(std::string_view parser, std::string_view source_set,
                 std::string_view locale, std::uint64_t first,
                 std::uint64_t count, bool complete,
                 const std::vector<Entry> &entries) {
  std::string output{"OFF-PRIVATE-L10N\0\1", 18};
  append_blob(output, parser);
  append_blob(output, source_set);
  append_blob(output, locale);
  append_u64(output, first);
  append_u64(output, count);
  output.push_back(complete ? '\1' : '\0');
  append_u64(output, entries.size());
  for (const auto &entry : entries) {
    append_blob(output, entry.id);
    append_blob(output, entry.text);
  }
  return output;
}
void write(const std::filesystem::path &path, std::string_view contents) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  check(static_cast<bool>(stream), "synthetic pack fixture writes");
}
off::ui::l10n::TranslationSourceBinding binding() {
  off::ui::l10n::RetailLocalizationMetadata metadata{
      "install.synthetic.v1", "parser.synthetic.v1", "source.synthetic.v1", 0U,
      3U};
  const auto result = off::ui::l10n::translation_source_binding(metadata);
  check(result.has_value(), "canonical synthetic metadata binds");
  return *result;
}
} // namespace

int main() {
  using namespace off::ui::l10n;
  try {
    const auto root = std::filesystem::path{OFF_TEST_WORK_DIR};
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    check(!error, "fixture directory exists");
    const auto source = binding();
    const auto one = "off.retail.source.synthetic.v1.1";
    const auto two = "off.retail.source.synthetic.v1.2";
    write(root / "swedish.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, false,
               {{one, "Projekttext ett"}}));
    const auto swedish =
        PrivateTranslationPack::load_local(root, "swedish.offl10n", source);
    check(swedish && !swedish->declared_complete() && swedish->find(one) &&
              !swedish->find(two),
          "partial local pack retains only opaque IDs and translated text");
    write(root / "english.offl10n",
          pack(source.parser_identity, source.source_set, "en", 0U, 3U, true,
               {{"off.retail.source.synthetic.v1.0", "Project text zero"},
                {one, "Project text one"},
                {two, "Project text two"}}));
    const auto english =
        PrivateTranslationPack::load_local(root, "english.offl10n", source);
    check(english && english->declared_complete(),
          "complete local pack covers exact canonical span");
    write(root / "en.offl10n",
          pack(source.parser_identity, source.source_set, "en", 0U, 3U, true,
               {{"off.retail.source.synthetic.v1.0", "Project text zero"},
                {one, "Project text one"},
                {two, "Project text two"}}));
    write(root / "sv.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, false,
               {{one, "Projekttext ett"}}));
    write(root / "de.offl10n",
          pack(source.parser_identity, source.source_set, "fr", 0U, 3U, false,
               {{one, "Texte du projet"}}));
    write(root / "rogue.offl10n",
          pack(source.parser_identity, source.source_set, "fr", 0U, 3U, false,
               {{one, "Texte du projet"}}));
    const auto enrolled = load_canonical_local_translation_packs(root, source);
    check(enrolled.size() == 2U && enrolled[0].locale() == "en" &&
              enrolled[1].locale() == "sv",
          "enrollment probes only canonical filenames and rejects filename "
          "locale mismatches");
    TranslationSourceBinding wide{source.parser_identity,
                                  "source.synthetic.wide.v1", 0U, 12U};
    std::vector<Entry> wide_entries;
    for (std::uint64_t ordinal{}; ordinal < wide.ordinal_count; ++ordinal)
      wide_entries.push_back(
          {make_retail_string_id(wide.source_set, ordinal), "Project text"});
    write(root / "wide.offl10n", pack(wide.parser_identity, wide.source_set,
                                      "en", 0U, 12U, true, wide_entries));
    check(PrivateTranslationPack::load_local(root, "wide.offl10n", wide)
              .has_value(),
          "complete packs validate ordinal coverage rather than lexical ID "
          "order");
    const auto resolver =
        PrivateTranslationResolver::build(source, {*swedish, *english});
    const std::string_view platform[] = {"de-DE", "sv-SE"};
    check(resolver &&
              resolver->resolve(one, "sv-SE", platform) == "Projekttext ett",
          "explicit locale resolves first");
    check(resolver->resolve(two, "sv-SE", platform) == "Project text two",
          "partial explicit pack falls through to English");
    check(resolver->resolve(one, "zz", platform) == "Projekttext ett",
          "system locale is used after unavailable explicit locale");
    write(root / "wrong-source.offl10n",
          pack(source.parser_identity, "source.synthetic.other", "sv", 0U, 3U,
               false, {{one, "X"}}));
    check(!PrivateTranslationPack::load_local(root, "wrong-source.offl10n",
                                              source),
          "source-set mismatch is rejected");
    write(root / "wrong-parser.offl10n",
          pack("parser.synthetic.other", source.source_set, "sv", 0U, 3U, false,
               {{one, "X"}}));
    check(!PrivateTranslationPack::load_local(root, "wrong-parser.offl10n",
                                              source),
          "parser mismatch is rejected");
    write(root / "wrong-span.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 1U, 2U, false,
               {{one, "X"}}));
    check(
        !PrivateTranslationPack::load_local(root, "wrong-span.offl10n", source),
        "span mismatch is rejected");
    write(root / "duplicate.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, false,
               {{one, "X"}, {one, "Y"}}));
    check(
        !PrivateTranslationPack::load_local(root, "duplicate.offl10n", source),
        "duplicate opaque IDs are rejected");
    write(root / "bad-id.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, false,
               {{"off.retail.source.synthetic.v1.03", "X"}}));
    check(!PrivateTranslationPack::load_local(root, "bad-id.offl10n", source),
          "noncanonical or out-of-span opaque IDs are rejected");
    write(root / "bad-utf8.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, false,
               {{one, std::string{"bad\xc3", 4}}}));
    check(!PrivateTranslationPack::load_local(root, "bad-utf8.offl10n", source),
          "invalid UTF-8 translated text is rejected");
    write(root / "bad-locale.offl10n",
          pack(source.parser_identity, source.source_set, "sv-SE", 0U, 3U,
               false, {{one, "X"}}));
    check(
        !PrivateTranslationPack::load_local(root, "bad-locale.offl10n", source),
        "pack locale must use a supported canonical tag");
    write(root / "incomplete-complete.offl10n",
          pack(source.parser_identity, source.source_set, "sv", 0U, 3U, true,
               {{one, "X"}}));
    check(!PrivateTranslationPack::load_local(
              root, "incomplete-complete.offl10n", source),
          "declared-complete packs reject missing canonical entries");
    check(
        !PrivateTranslationPack::load_local(root, "../english.offl10n", source),
        "loader rejects paths outside local pack directory");
    RetailLocalizationMetadata malformed{"install.synthetic.v1",
                                         source.parser_identity,
                                         source.source_set, 1U, 1U};
    check(!translation_source_binding(malformed),
          "noncanonical extracted metadata cannot bind a pack");
    std::filesystem::remove_all(root, error);
    std::cout << "private translation pack tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
