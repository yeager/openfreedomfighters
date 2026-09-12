#include "off/ui/private_translation_pack.hpp"

#include "off/platform/locale_preferences.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <ranges>

namespace off::ui::l10n {
namespace {
constexpr std::string_view magic{"OFF-PRIVATE-L10N\0\1", 18};
constexpr std::size_t maximum_file_bytes = 128U << 20U;
constexpr std::size_t maximum_entries = 1'000'000U;
constexpr std::size_t maximum_text_bytes = 1U << 20U;

bool valid_identifier(std::string_view value, std::size_t maximum) noexcept {
  return !value.empty() && value.size() <= maximum &&
         std::ranges::all_of(value, [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
         });
}
bool valid_utf8(std::string_view value) noexcept {
  for (std::size_t cursor{}; cursor < value.size();) {
    const auto lead = static_cast<unsigned char>(value[cursor]);
    if (lead <= 0x7fU) {
      ++cursor;
      continue;
    }
    const unsigned length = lead >= 0xc2U && lead <= 0xdfU   ? 2U
                            : lead >= 0xe0U && lead <= 0xefU ? 3U
                            : lead >= 0xf0U && lead <= 0xf4U ? 4U
                                                             : 0U;
    if (!length || length > value.size() - cursor)
      return false;
    std::uint32_t point = lead & (length == 2U   ? 0x1fU
                                  : length == 3U ? 0x0fU
                                                 : 0x07U);
    for (unsigned index = 1U; index < length; ++index) {
      const auto byte = static_cast<unsigned char>(value[cursor + index]);
      if ((byte & 0xc0U) != 0x80U)
        return false;
      point = (point << 6U) | (byte & 0x3fU);
    }
    const auto minimum = length == 2U   ? 0x80U
                         : length == 3U ? 0x800U
                                        : 0x10000U;
    if (point < minimum || point > 0x10ffffU ||
        (point >= 0xd800U && point <= 0xdfffU))
      return false;
    cursor += length;
  }
  return true;
}
constexpr std::array<std::string_view, 20> canonical_pack_locales{
    "en", "sv", "da", "nb", "fi", "de", "fr", "es", "it", "pt-BR",
    "pl", "cs", "hu", "ro", "tr", "ru", "uk", "ja", "ko", "zh-Hans"};
bool valid_locale(std::string_view locale) noexcept {
  return std::ranges::find(canonical_pack_locales, locale) !=
         canonical_pack_locales.end();
}
std::optional<std::string_view>
canonical_locale(std::string_view value) noexcept {
  // SDL reports well-formed BCP-47 components on supported platforms, but an
  // explicit command-line locale and other host providers are less uniform.
  // Normalize once through the shared host parser before making a supported
  // pack decision. In particular, a differently cased Traditional Chinese
  // tag must never be mistaken for Simplified Chinese.
  const auto normalized = platform::canonical_host_locale_tag(value);
  if (!normalized)
    return std::nullopt;
  value = *normalized;
  const auto language_end = value.find('-');
  const auto language = value.substr(0, language_end);
  const auto equal_ascii = [](std::string_view left, std::string_view right) {
    if (left.size() != right.size())
      return false;
    for (std::size_t index{}; index < left.size(); ++index) {
      const auto lower = [](char character) {
        return character >= 'A' && character <= 'Z'
                   ? static_cast<char>(character - 'A' + 'a')
                   : character;
      };
      if (lower(left[index]) != lower(right[index]))
        return false;
    }
    return true;
  };
  if (equal_ascii(language, "en"))
    return "en";
  if (equal_ascii(language, "sv"))
    return "sv";
  if (equal_ascii(language, "da"))
    return "da";
  if (equal_ascii(language, "nb") || equal_ascii(language, "no"))
    return "nb";
  if (equal_ascii(language, "fi"))
    return "fi";
  if (equal_ascii(language, "de"))
    return "de";
  if (equal_ascii(language, "fr"))
    return "fr";
  if (equal_ascii(language, "es"))
    return "es";
  if (equal_ascii(language, "it"))
    return "it";
  if (equal_ascii(language, "pt"))
    return "pt-BR";
  if (equal_ascii(language, "pl"))
    return "pl";
  if (equal_ascii(language, "cs"))
    return "cs";
  if (equal_ascii(language, "hu"))
    return "hu";
  if (equal_ascii(language, "ro"))
    return "ro";
  if (equal_ascii(language, "tr"))
    return "tr";
  if (equal_ascii(language, "ru"))
    return "ru";
  if (equal_ascii(language, "uk"))
    return "uk";
  if (equal_ascii(language, "ja"))
    return "ja";
  if (equal_ascii(language, "ko"))
    return "ko";
  if (equal_ascii(language, "zh")) {
    // Treat script and region as complete canonical subtags.  A substring
    // test here would turn an unrelated variant (for example, "twinkle")
    // into a Traditional-Chinese request, while accepting an explicitly
    // incompatible script would silently select the Simplified pack.
    std::size_t begin = language_end;
    while (begin != std::string_view::npos && begin < value.size()) {
      ++begin;
      const auto end = value.find('-', begin);
      const auto subtag = value.substr(begin, end - begin);
      if (subtag.empty())
        return std::nullopt;
      const auto is_alpha = std::ranges::all_of(subtag, [](char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z');
      });
      const bool is_script = subtag.size() == 4U && is_alpha;
      if ((is_script && !equal_ascii(subtag, "Hans")) ||
          equal_ascii(subtag, "Hant") || equal_ascii(subtag, "TW") ||
          equal_ascii(subtag, "HK") || equal_ascii(subtag, "MO"))
        return std::nullopt;
      begin = end;
    }
    return "zh-Hans";
  }
  return std::nullopt;
}
bool valid_binding(const TranslationSourceBinding &binding) noexcept {
  return valid_identifier(binding.parser_identity, 128U) &&
         valid_identifier(binding.source_set, 96U) &&
         binding.ordinal_count > 0U && binding.first_ordinal == 0U;
}
bool valid_filename(std::string_view filename) noexcept {
  return filename.size() > 5U && filename.size() <= 128U &&
         filename.ends_with(".offl10n") &&
         std::ranges::all_of(filename, [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
         });
}
bool regular_non_link(const std::filesystem::path &path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) &&
         !std::filesystem::is_symlink(status);
}
bool directory_non_link(const std::filesystem::path &path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) &&
         !std::filesystem::is_symlink(status);
}
std::optional<std::uint64_t> read_u64(std::string_view input,
                                      std::size_t &cursor) {
  if (cursor > input.size() || 8U > input.size() - cursor)
    return std::nullopt;
  std::uint64_t value{};
  for (unsigned byte{}; byte < 8U; ++byte)
    value |=
        static_cast<std::uint64_t>(static_cast<unsigned char>(input[cursor++]))
        << (byte * 8U);
  return value;
}
std::optional<std::string> read_blob(std::string_view input,
                                     std::size_t &cursor, std::size_t maximum) {
  const auto size = read_u64(input, cursor);
  if (!size || *size > maximum || *size > input.size() - cursor)
    return std::nullopt;
  std::string result{input.substr(cursor, static_cast<std::size_t>(*size))};
  cursor += static_cast<std::size_t>(*size);
  return result;
}
std::optional<std::uint64_t>
ordinal_for_id(std::string_view id,
               const TranslationSourceBinding &binding) noexcept {
  const auto prefix = "off.retail." + binding.source_set + ".";
  if (!id.starts_with(prefix))
    return std::nullopt;
  const auto tail = id.substr(prefix.size());
  if (tail.empty() || (tail.size() > 1U && tail.front() == '0'))
    return std::nullopt;
  std::uint64_t ordinal{};
  for (char character : tail) {
    if (character < '0' || character > '9')
      return std::nullopt;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (ordinal > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
      return std::nullopt;
    ordinal = ordinal * 10U + digit;
  }
  if (ordinal < binding.first_ordinal ||
      ordinal - binding.first_ordinal >= binding.ordinal_count)
    return std::nullopt;
  return ordinal;
}
} // namespace

std::optional<PrivateTranslationPack>
PrivateTranslationPack::decode(std::string_view bytes,
                               const TranslationSourceBinding &binding) {
  if (!valid_binding(binding) || !bytes.starts_with(magic))
    return std::nullopt;
  std::size_t cursor = magic.size();
  auto parser = read_blob(bytes, cursor, 128U);
  auto source_set = read_blob(bytes, cursor, 96U);
  auto locale = read_blob(bytes, cursor, 16U);
  const auto first = read_u64(bytes, cursor);
  const auto count = read_u64(bytes, cursor);
  if (!parser || !source_set || !locale || !first || !count ||
      cursor >= bytes.size())
    return std::nullopt;
  const auto complete = static_cast<unsigned char>(bytes[cursor++]);
  const auto entry_count = read_u64(bytes, cursor);
  if (complete > 1U || !entry_count || *entry_count == 0U ||
      *entry_count > maximum_entries || *parser != binding.parser_identity ||
      *source_set != binding.source_set || *first != binding.first_ordinal ||
      *count != binding.ordinal_count || !valid_locale(*locale))
    return std::nullopt;
  PrivateTranslationPack result;
  result.binding_ = binding;
  result.locale_ = std::move(*locale);
  result.complete_ = complete == 1U;
  result.entries_.reserve(static_cast<std::size_t>(*entry_count));
  for (std::uint64_t index{}; index < *entry_count; ++index) {
    auto id = read_blob(bytes, cursor, 256U);
    auto text = read_blob(bytes, cursor, maximum_text_bytes);
    if (!id || !text || text->empty() ||
        text->find('\0') != std::string::npos || !valid_utf8(*text) ||
        !ordinal_for_id(*id, binding))
      return std::nullopt;
    result.entries_.push_back({std::move(*id), std::move(*text)});
  }
  if (cursor != bytes.size())
    return std::nullopt;
  std::ranges::sort(result.entries_, {}, &PrivateTranslationEntry::id);
  if (std::ranges::adjacent_find(result.entries_, {},
                                 &PrivateTranslationEntry::id) !=
      result.entries_.end())
    return std::nullopt;
  if (result.complete_) {
    if (result.entries_.size() != binding.ordinal_count)
      return std::nullopt;
    for (std::uint64_t index{}; index < binding.ordinal_count; ++index) {
      const auto expected = make_retail_string_id(
          binding.source_set, binding.first_ordinal + index);
      const auto found = std::ranges::lower_bound(result.entries_, expected, {},
                                                  &PrivateTranslationEntry::id);
      if (found == result.entries_.end() || found->id != expected)
        return std::nullopt;
    }
  }
  return result;
}

std::optional<TranslationSourceBinding>
translation_source_binding(const RetailLocalizationMetadata &metadata) {
  if (!valid_identifier(metadata.parser_identity, 128U) ||
      !valid_identifier(metadata.source_set, 96U) ||
      metadata.first_ordinal != 0U || metadata.ordinal_count == 0U ||
      metadata.ordinal_count > maximum_entries)
    return std::nullopt;
  return TranslationSourceBinding{metadata.parser_identity, metadata.source_set,
                                  metadata.first_ordinal,
                                  metadata.ordinal_count};
}
std::optional<PrivateTranslationPack>
PrivateTranslationPack::load_local(const std::filesystem::path &local_directory,
                                   std::string_view filename,
                                   const TranslationSourceBinding &binding) {
  if (local_directory.empty() || !valid_filename(filename) ||
      !valid_binding(binding) || !directory_non_link(local_directory))
    return std::nullopt;
  const auto path = local_directory / std::string{filename};
  if (path.parent_path() != local_directory || !regular_non_link(path))
    return std::nullopt;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > maximum_file_bytes)
    return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
      input.peek() != std::char_traits<char>::eof())
    return std::nullopt;
  return decode(bytes, binding);
}
std::vector<PrivateTranslationPack> load_canonical_local_translation_packs(
    const std::filesystem::path &local_directory,
    const TranslationSourceBinding &binding) {
  std::vector<PrivateTranslationPack> accepted;
  if (!valid_binding(binding) || !directory_non_link(local_directory))
    return accepted;
  accepted.reserve(canonical_pack_locales.size());
  for (const auto locale : canonical_pack_locales) {
    const auto filename = std::string{locale} + ".offl10n";
    auto pack =
        PrivateTranslationPack::load_local(local_directory, filename, binding);
    if (pack && pack->locale() == locale)
      accepted.push_back(std::move(*pack));
  }
  return accepted;
}
std::optional<std::string_view>
PrivateTranslationPack::find(std::string_view id) const noexcept {
  const auto found =
      std::ranges::lower_bound(entries_, id, {}, &PrivateTranslationEntry::id);
  if (found == entries_.end() || found->id != id)
    return std::nullopt;
  return found->text;
}
std::optional<PrivateTranslationResolver>
PrivateTranslationResolver::build(TranslationSourceBinding binding,
                                  std::vector<PrivateTranslationPack> packs) {
  if (!valid_binding(binding) || packs.empty())
    return std::nullopt;
  for (const auto &pack : packs)
    if (pack.binding_ != binding || !valid_locale(pack.locale_))
      return std::nullopt;
  std::ranges::sort(packs, {}, &PrivateTranslationPack::locale_);
  if (std::ranges::adjacent_find(packs, {}, &PrivateTranslationPack::locale_) !=
      packs.end())
    return std::nullopt;
  PrivateTranslationResolver result;
  result.binding_ = std::move(binding);
  result.packs_ = std::move(packs);
  return result;
}
std::optional<std::string_view> PrivateTranslationResolver::resolve(
    std::string_view id, std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales) const noexcept {
  if (!ordinal_for_id(id, binding_))
    return std::nullopt;
  std::vector<std::string_view> preferences;
  const auto append = [&](std::string_view candidate) {
    if (const auto canonical = canonical_locale(candidate);
        canonical &&
        std::ranges::find(preferences, *canonical) == preferences.end())
      preferences.push_back(*canonical);
  };
  append(explicit_locale);
  for (const auto locale : platform_locales)
    append(locale);
  append("en");
  for (const auto locale : preferences) {
    const auto pack = std::ranges::lower_bound(
        packs_, locale, {}, &PrivateTranslationPack::locale_);
    if (pack != packs_.end() && pack->locale_ == locale)
      if (const auto text = pack->find(id))
        return text;
  }
  return std::nullopt;
}
} // namespace off::ui::l10n
