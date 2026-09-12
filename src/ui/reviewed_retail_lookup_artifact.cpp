#include "off/ui/reviewed_retail_lookup_artifact.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <ranges>
#include <string>

namespace off::ui::l10n {
namespace {
constexpr std::string_view magic{"OFF-REVIEWED-RETAIL-LOOKUP\0\1", 28};
constexpr std::string_view filename{"reviewed-retail-lookup.offlookup"};
constexpr std::size_t maximum_file_bytes = 16U << 20U;
constexpr std::size_t maximum_observations = 1'000'000U;

bool valid_identifier(std::string_view value, std::size_t maximum) noexcept {
  return !value.empty() && value.size() <= maximum &&
         std::ranges::all_of(value, [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
         });
}
bool valid_binding(const TranslationSourceBinding &binding) noexcept {
  return valid_identifier(binding.parser_identity, 128U) &&
         valid_identifier(binding.source_set, 96U) &&
         binding.first_ordinal == 0U && binding.ordinal_count > 0U;
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
    value |= static_cast<std::uint64_t>(
                 static_cast<unsigned char>(input[cursor++])) <<
             (byte * 8U);
  return value;
}
std::optional<std::string> read_blob(std::string_view input,
                                     std::size_t &cursor,
                                     std::size_t maximum) {
  const auto size = read_u64(input, cursor);
  if (!size || *size > maximum || *size > input.size() - cursor)
    return std::nullopt;
  std::string result{input.substr(cursor, static_cast<std::size_t>(*size))};
  cursor += static_cast<std::size_t>(*size);
  return result;
}
} // namespace

std::vector<ReviewedRetailLookupArtifact>
load_local_reviewed_retail_lookup_artifacts(
    const std::filesystem::path &local_directory,
    const TranslationSourceBinding &binding) {
  if (local_directory.empty() || !valid_binding(binding) ||
      !directory_non_link(local_directory))
    return {};
  const auto path = local_directory / std::string{filename};
  if (path.parent_path() != local_directory || !regular_non_link(path))
    return {};
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > maximum_file_bytes)
    return {};
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
      input.peek() != std::char_traits<char>::eof() || !bytes.starts_with(magic))
    return {};
  std::size_t cursor = magic.size();
  const auto parser = read_blob(bytes, cursor, 128U);
  const auto source_set = read_blob(bytes, cursor, 96U);
  const auto first_ordinal = read_u64(bytes, cursor);
  const auto ordinal_count = read_u64(bytes, cursor);
  const auto observation_count = read_u64(bytes, cursor);
  if (!parser || !source_set || !first_ordinal || !ordinal_count ||
      !observation_count || *observation_count == 0U ||
      *observation_count > maximum_observations ||
      TranslationSourceBinding{*parser, *source_set, *first_ordinal,
                               *ordinal_count} != binding)
    return {};
  std::vector<ReviewedRetailLookupObservation> observations;
  observations.reserve(static_cast<std::size_t>(*observation_count));
  for (std::uint64_t index{}; index < *observation_count; ++index) {
    const auto site = read_blob(bytes, cursor, 128U);
    const auto ordinal = read_u64(bytes, cursor);
    if (!site || !ordinal)
      return {};
    observations.push_back({std::move(*site), *ordinal});
  }
  if (cursor != bytes.size())
    return {};
  const auto artifact = ReviewedRetailLookupArtifact::reviewed(
      binding, std::move(observations));
  if (!artifact)
    return {};
  return {*artifact};
}

} // namespace off::ui::l10n
