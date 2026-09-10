#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace off::data {

// A bounded inventory of printable, NUL-terminated byte runs in a LOC member.
// This intentionally does not assign key/text or language semantics: those
// require a recovered LOC grammar. It is a lossless discovery boundary for
// subsequent real-data observations, not a localization-table parser.
struct LocStringCandidate final {
  std::size_t offset{};
  std::string_view bytes{};
  // Lexical observation only. True means every byte is ASCII alphanumeric or
  // underscore. It is not a claim that the field is a localization key.
  bool ascii_identifier_like{};
};

class LocStringIndex final {
public:
  [[nodiscard]] static LocStringIndex scan(std::span<const std::byte> bytes);
  [[nodiscard]] std::span<const LocStringCandidate> candidates() const noexcept {
    return candidates_;
  }

private:
  std::vector<LocStringCandidate> candidates_;
};

}  // namespace off::data
