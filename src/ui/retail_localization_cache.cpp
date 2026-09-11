#include "off/ui/retail_localization_cache.hpp"

#include "off/crypto/sha256.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <random>
#include <ranges>

namespace off::ui::l10n {
namespace {
constexpr std::string_view magic{"OFF-RETAIL-L10N\0\1", 17};
constexpr std::size_t maximum_strings = 1'000'000U;
constexpr std::size_t maximum_text_bytes = 1U << 20U;
constexpr std::size_t maximum_catalog_bytes = 128U << 20U;

bool regular_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status);
}
bool directory_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
bool valid_identifier(std::string_view value) noexcept {
  return !value.empty() && value.size() <= 256U && std::ranges::all_of(value, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
  });
}
bool valid_utf8(std::string_view value) noexcept {
  for (std::size_t cursor{}; cursor < value.size();) {
    const auto lead = static_cast<unsigned char>(value[cursor]);
    if (lead <= 0x7fU) { ++cursor; continue; }
    const unsigned length = lead >= 0xc2U && lead <= 0xdfU ? 2U : lead >= 0xe0U && lead <= 0xefU ? 3U : lead >= 0xf0U && lead <= 0xf4U ? 4U : 0U;
    if (length == 0U || length > value.size() - cursor) return false;
    std::uint32_t code_point = lead & (length == 2U ? 0x1fU : length == 3U ? 0x0fU : 0x07U);
    for (unsigned index = 1U; index < length; ++index) {
      const auto byte = static_cast<unsigned char>(value[cursor + index]);
      if ((byte & 0xc0U) != 0x80U) return false;
      code_point = (code_point << 6U) | (byte & 0x3fU);
    }
    const auto minimum = length == 2U ? 0x80U : length == 3U ? 0x800U : 0x10000U;
    if (code_point < minimum || code_point > 0x10ffffU || (code_point >= 0xd800U && code_point <= 0xdfffU)) return false;
    cursor += length;
  }
  return true;
}
bool valid_source_text(std::string_view value) noexcept {
  return value.size() <= maximum_text_bytes && value.find('\0') == std::string_view::npos && valid_utf8(value);
}
bool valid_translation_text(std::string_view value) noexcept {
  return !value.empty() && valid_source_text(value);
}
bool valid_snapshot(const RetailLocalizationSnapshot& snapshot) noexcept {
  if (snapshot.installation_identity.size() > 128U || snapshot.parser_identity.size() > 128U ||
      snapshot.source_set.size() > 96U || !valid_identifier(snapshot.installation_identity) || !valid_identifier(snapshot.parser_identity) || !valid_identifier(snapshot.source_set) ||
      snapshot.strings.empty() || snapshot.strings.size() > maximum_strings) return false;
  std::size_t total = magic.size() + 8U + snapshot.installation_identity.size() + 8U +
                      snapshot.parser_identity.size() + 8U + snapshot.source_set.size() + 8U;
  if (total > maximum_catalog_bytes) return false;
  for (std::size_t index{}; index < snapshot.strings.size(); ++index) {
    if (snapshot.strings[index].ordinal != index || !valid_source_text(snapshot.strings[index].english) ||
        total > maximum_catalog_bytes - 16U ||
        snapshot.strings[index].english.size() > maximum_catalog_bytes - total - 16U) return false;
    total += 16U + snapshot.strings[index].english.size();
  }
  return true;
}
void append_u64(std::string& out, std::uint64_t value) {
  for (unsigned byte{}; byte < sizeof(value); ++byte) out.push_back(static_cast<char>((value >> (byte * 8U)) & 0xffU));
}
std::optional<std::uint64_t> read_u64(std::string_view input, std::size_t& cursor) {
  if (cursor > input.size() || sizeof(std::uint64_t) > input.size() - cursor) return std::nullopt;
  std::uint64_t value{};
  for (unsigned byte{}; byte < sizeof(value); ++byte)
    value |= static_cast<std::uint64_t>(static_cast<unsigned char>(input[cursor++])) << (byte * 8U);
  return value;
}
void append_blob(std::string& out, std::string_view value) { append_u64(out, value.size()); out.append(value); }
std::optional<std::string> read_blob(std::string_view input, std::size_t& cursor, std::size_t maximum) {
  const auto size = read_u64(input, cursor);
  if (!size || *size > maximum || *size > input.size() - cursor) return std::nullopt;
  std::string result{input.substr(cursor, static_cast<std::size_t>(*size))};
  cursor += static_cast<std::size_t>(*size);
  return result;
}
std::string encode(const RetailLocalizationSnapshot& snapshot) {
  std::string out{magic};
  append_blob(out, snapshot.installation_identity); append_blob(out, snapshot.parser_identity); append_blob(out, snapshot.source_set);
  append_u64(out, snapshot.strings.size());
  for (const auto& string : snapshot.strings) { append_u64(out, string.ordinal); append_blob(out, string.english); }
  return out;
}
std::optional<RetailLocalizationSnapshot> decode(std::string_view input) {
  if (!input.starts_with(magic)) return std::nullopt;
  std::size_t cursor = magic.size();
  auto installation = read_blob(input, cursor, 128U); auto parser = read_blob(input, cursor, 128U); auto source_set = read_blob(input, cursor, 96U);
  const auto count = read_u64(input, cursor);
  if (!installation || !parser || !source_set || !count || *count == 0U || *count > maximum_strings) return std::nullopt;
  RetailLocalizationSnapshot result{std::move(*installation), std::move(*parser), std::move(*source_set), {}};
  result.strings.reserve(static_cast<std::size_t>(*count));
  for (std::uint64_t index{}; index < *count; ++index) {
    const auto ordinal = read_u64(input, cursor); auto english = read_blob(input, cursor, maximum_text_bytes);
    if (!ordinal || !english) return std::nullopt;
    result.strings.push_back({*ordinal, std::move(*english)});
  }
  if (cursor != input.size() || !valid_snapshot(result)) return std::nullopt;
  return result;
}
std::filesystem::path cache_path(const std::filesystem::path& root, std::string_view installation,
                                 std::string_view parser, std::string_view source_set) {
  return root / "retail-localization" /
      (crypto::to_hex(crypto::sha256(std::string{installation} + "\n" + std::string{parser} + "\n" + std::string{source_set})) + ".bin");
}
std::optional<RetailLocalizationSnapshot> load(const std::filesystem::path& path) {
  if (!regular_non_link(path)) return std::nullopt;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > maximum_catalog_bytes) return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  std::string encoded(static_cast<std::size_t>(size), '\0');
  input.read(encoded.data(), static_cast<std::streamsize>(encoded.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(encoded.size())) return std::nullopt;
  if (input.peek() != std::char_traits<char>::eof()) return std::nullopt;
  return decode(encoded);
}
bool store(const std::filesystem::path& path, const RetailLocalizationSnapshot& snapshot) {
  std::error_code error; const auto parent = path.parent_path(); const auto root = parent.parent_path();
  std::filesystem::create_directories(root, error);
  if (error || !directory_non_link(root)) return false;
  std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, error);
  if (error) return false;
  std::filesystem::create_directory(parent, error);
  if (error || !directory_non_link(parent)) return false;
  error.clear();
  std::filesystem::permissions(parent, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, error);
  if (error || std::filesystem::exists(path, error) || error) return false;
  const auto contents = encode(snapshot);
  static std::atomic_uint64_t staging_counter{};
  std::random_device random;
  const auto nonce = crypto::to_hex(crypto::sha256(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ":" + std::to_string(random()) + ":" + std::to_string(staging_counter++)));
  const auto staging_directory = parent / (".staging-" + nonce);
  if (!std::filesystem::create_directory(staging_directory, error) || error) return false;
  std::filesystem::permissions(staging_directory, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, error);
  if (error) { std::filesystem::remove(staging_directory, error); return false; }
  const auto staging = staging_directory / "catalog.bin";
  { std::ofstream output(staging, std::ios::binary | std::ios::trunc); if (!output) { std::filesystem::remove_all(staging_directory, error); return false; }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size())); output.flush(); if (!output) { std::filesystem::remove_all(staging_directory, error); return false; } }
  std::filesystem::permissions(staging, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, error);
  if (error) { std::filesystem::remove_all(staging_directory, error); return false; }
  const auto verified = load(staging);
  if (!verified || *verified != snapshot) { std::filesystem::remove_all(staging_directory, error); return false; }
  std::filesystem::rename(staging, path, error);
  const bool renamed = !error;
  std::filesystem::remove_all(staging_directory, error);
  return renamed;
}
} // namespace

std::string make_retail_string_id(std::string_view source_set, std::uint64_t ordinal) {
  if (!valid_identifier(source_set) || source_set.size() > 96U) return {};
  return "off.retail." + std::string{source_set} + "." + std::to_string(ordinal);
}
std::optional<RetailTranslationCatalog> RetailTranslationCatalog::build(std::vector<RetailTranslationEntry> entries) {
  if (entries.empty()) return std::nullopt;
  for (const auto& entry : entries) if (!valid_identifier(entry.id) || !valid_translation_text(entry.text)) return std::nullopt;
  std::ranges::sort(entries, {}, &RetailTranslationEntry::id);
  if (std::ranges::adjacent_find(entries, {}, &RetailTranslationEntry::id) != entries.end()) return std::nullopt;
  RetailTranslationCatalog result; result.entries_ = std::move(entries); return result;
}
std::optional<std::string_view> RetailTranslationCatalog::find(std::string_view id) const noexcept {
  const auto found = std::ranges::lower_bound(entries_, id, {}, &RetailTranslationEntry::id);
  if (found == entries_.end() || found->id != id) return std::nullopt;
  return found->text;
}
RetailLocalizationCacheResult ensure_retail_localization_snapshot(
    const std::filesystem::path& root, std::string_view installation_identity, std::string_view parser_identity, std::string_view source_set,
    const std::function<std::optional<std::vector<RetailSourceString>>()>& extract) {
  if (root.empty() || !valid_identifier(installation_identity) || !valid_identifier(parser_identity) || !valid_identifier(source_set) || source_set.size() > 96U || !extract)
    return {.status = RetailLocalizationCacheStatus::unavailable, .snapshot = std::nullopt};
  std::error_code root_error;
  std::filesystem::create_directories(root, root_error);
  const auto directory = root / "retail-localization";
  if (root_error || !directory_non_link(root) ||
      (std::filesystem::exists(directory, root_error) &&
       (root_error || !directory_non_link(directory))))
    return {.status = RetailLocalizationCacheStatus::unavailable, .snapshot = std::nullopt};
  const auto path = cache_path(root, installation_identity, parser_identity, source_set);
  if (const auto existing = load(path); existing && existing->installation_identity == installation_identity && existing->parser_identity == parser_identity && existing->source_set == source_set)
    return {.status = RetailLocalizationCacheStatus::loaded, .snapshot = *existing};
  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    if (error || !regular_non_link(path) || !std::filesystem::remove(path, error) || error)
      return {.status = RetailLocalizationCacheStatus::unavailable, .snapshot = std::nullopt};
  }
  const auto extracted = extract(); if (!extracted) return {.status = RetailLocalizationCacheStatus::unavailable, .snapshot = std::nullopt};
  RetailLocalizationSnapshot snapshot{std::string{installation_identity}, std::string{parser_identity}, std::string{source_set}, *extracted};
  if (!valid_snapshot(snapshot)) return {.status = RetailLocalizationCacheStatus::invalid, .snapshot = std::nullopt};
  if (!store(path, snapshot)) return {.status = RetailLocalizationCacheStatus::unavailable, .snapshot = std::nullopt};
  return {.status = RetailLocalizationCacheStatus::extracted, .snapshot = std::move(snapshot)};
}
} // namespace off::ui::l10n
