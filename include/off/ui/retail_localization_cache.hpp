#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace off::ui::l10n {

// A recovered LOC reader supplies these records only after it has established
// that `english` is display text and that `ordinal` is its canonical order in
// a supported installation. There is deliberately no LocStringIndex adapter:
// its candidates are unclassified byte runs, not approved decoder output.
struct RetailSourceString final {
  std::uint64_t ordinal{};
  std::string english;
  bool operator==(const RetailSourceString&) const = default;
};

// The stable ID is derived from the parser's versioned source-set identifier
// and canonical ordinal, never from English text. Translation packs can
// therefore contain only this ID and independently supplied translated text.
[[nodiscard]] std::string make_retail_string_id(std::string_view source_set,
                                                 std::uint64_t ordinal);

struct RetailTranslationEntry final {
  std::string id;
  std::string text;
};

// Project translation data intentionally has no English-source field. Its
// text still requires a contributor licence; the absence of a source column
// is not a copyright determination.
class RetailTranslationCatalog final {
public:
  [[nodiscard]] static std::optional<RetailTranslationCatalog>
  build(std::vector<RetailTranslationEntry> entries);
  // Use this for a translation pack associated with a private extracted
  // catalog. The input contains its text-free source-set identity and record
  // count only; it does not accept English source text. Every entry must name
  // one canonical ID in that exact source set and ordinal range.
  [[nodiscard]] static std::optional<RetailTranslationCatalog>
  build_for_source_set(std::string_view source_set, std::uint64_t record_count,
                       std::vector<RetailTranslationEntry> entries);
  [[nodiscard]] std::optional<std::string_view>
  find(std::string_view id) const noexcept;
private:
  std::vector<RetailTranslationEntry> entries_;
};

struct RetailLocalizationSnapshot final {
  std::string installation_identity;
  std::string parser_identity;
  std::string source_set;
  std::vector<RetailSourceString> strings;
  bool operator==(const RetailLocalizationSnapshot&) const = default;
};

enum class RetailLocalizationCacheStatus : std::uint8_t {
  loaded, extracted, unavailable, invalid,
};

struct RetailLocalizationCacheResult final {
  RetailLocalizationCacheStatus status{RetailLocalizationCacheStatus::unavailable};
  std::optional<RetailLocalizationSnapshot> snapshot;
};

// The cache is private per-user state. It is never a source of install
// verification and never writes to the game-data folder. `extract` is invoked
// only on a cache miss. Callers must not pass an unrecovered LOC byte scanner.
[[nodiscard]] RetailLocalizationCacheResult ensure_retail_localization_snapshot(
    const std::filesystem::path& cache_root, std::string_view installation_identity,
    std::string_view parser_identity, std::string_view source_set,
    const std::function<std::optional<std::vector<RetailSourceString>>()>& extract);

} // namespace off::ui::l10n
