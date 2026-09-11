#pragma once

#include <cstddef>
#include <cstdint>
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

// Aggregate structural evidence for a LOC member. It deliberately excludes
// every source byte and candidate string, so it can be recorded publicly while
// text and legacy encoding remain private owner data.
struct LocStringProfile final {
  std::size_t member_bytes{};
  std::size_t candidate_count{};
  std::size_t ascii_identifier_candidate_count{};
  std::size_t candidate_bytes{};
  std::size_t maximum_candidate_bytes{};
  std::uint64_t structure_digest{14695981039346656037ULL};

  [[nodiscard]] bool operator==(const LocStringProfile&) const = default;
};

class LocStringIndex final {
public:
  [[nodiscard]] static LocStringIndex scan(std::span<const std::byte> bytes);
  [[nodiscard]] static LocStringProfile profile(std::span<const std::byte> bytes);
  [[nodiscard]] std::span<const LocStringCandidate> candidates() const noexcept {
    return candidates_;
  }

private:
  std::vector<LocStringCandidate> candidates_;
};

}  // namespace off::data
