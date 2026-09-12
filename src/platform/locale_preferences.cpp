#include "off/platform/locale_preferences.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace off::platform {
namespace {

constexpr std::size_t max_locale_tag_size = 35U;

bool ascii_alpha(char value) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

char ascii_lower(char value) noexcept {
  return value >= 'A' && value <= 'Z'
             ? static_cast<char>(value - 'A' + 'a')
             : value;
}

char ascii_upper(char value) noexcept {
  return value >= 'a' && value <= 'z'
             ? static_cast<char>(value - 'a' + 'A')
             : value;
}

bool all_alpha(std::string_view value) noexcept {
  return std::ranges::all_of(value, ascii_alpha);
}

bool all_digit(std::string_view value) noexcept {
  return std::ranges::all_of(value, ascii_digit);
}

} // namespace

std::optional<std::string>
canonical_host_locale_tag(std::string_view value) noexcept {
  if (value.empty() || value.size() > max_locale_tag_size)
    return std::nullopt;
  // POSIX spellings may carry an encoding suffix. It is not a locale
  // preference, so drop it only when it is a conventional ASCII token.
  const auto encoding = value.find('.');
  if (encoding != std::string_view::npos) {
    const auto suffix = value.substr(encoding + 1U);
    if (suffix.empty() || !std::ranges::all_of(
                              suffix, [](char c) noexcept {
                                return ascii_alpha(c) || ascii_digit(c) ||
                                       c == '-';
                              }))
      return std::nullopt;
    value = value.substr(0U, encoding);
  }
  if (value.empty())
    return std::nullopt;

  std::array<std::string_view, max_locale_tag_size / 2U + 1U> pieces{};
  std::size_t piece_count = 0U;
  std::size_t begin = 0U;
  while (begin < value.size()) {
    const auto separator = value.find_first_of("-_", begin);
    const auto end = separator == std::string_view::npos ? value.size() : separator;
    const auto piece = value.substr(begin, end - begin);
    if (piece.empty() ||
        (!std::ranges::all_of(piece, ascii_alpha) &&
         !std::ranges::all_of(piece, ascii_digit)) ||
        piece_count == pieces.size())
      return std::nullopt;
    pieces[piece_count++] = piece;
    if (separator == std::string_view::npos)
      break;
    begin = separator + 1U;
  }
  if (piece_count == 0U || pieces.front().size() < 2U ||
      pieces.front().size() > 8U || !all_alpha(pieces.front()))
    return std::nullopt;

  std::string result;
  result.reserve(value.size());
  for (const auto character : pieces.front())
    result.push_back(ascii_lower(character));
  for (std::size_t index = 1U; index < piece_count; ++index) {
    const auto piece = pieces[index];
    const bool script = piece.size() == 4U && all_alpha(piece);
    const bool region = (piece.size() == 2U && all_alpha(piece)) ||
                        (piece.size() == 3U && all_digit(piece));
    const bool variant = piece.size() >= 4U && piece.size() <= 8U &&
                         (all_alpha(piece) || all_digit(piece));
    if (!script && !region && !variant)
      return std::nullopt;
    result.push_back('-');
    for (std::size_t character = 0U; character < piece.size(); ++character) {
      result.push_back(script ? (character == 0U ? ascii_upper(piece[character])
                                                  : ascii_lower(piece[character]))
                              : (region ? ascii_upper(piece[character])
                                        : ascii_lower(piece[character])));
    }
  }
  return result;
}

std::vector<std::string>
canonical_host_locale_preferences(std::span<const std::string_view> values) {
  std::vector<std::string> result;
  result.reserve(values.size());
  for (const auto value : values) {
    const auto canonical = canonical_host_locale_tag(value);
    if (!canonical || std::ranges::find(result, *canonical) != result.end())
      continue;
    result.push_back(*canonical);
  }
  return result;
}

} // namespace off::platform
