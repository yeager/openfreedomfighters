#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

// This is a local decoder for the verified Steam-English LOC corpus.  Its
// output remains user-private: callers must not log, serialize into a source
// tree, or upload the returned strings.
inline constexpr std::string_view loc_catalog_parser_identity{"loc-grammar-v1"};

struct LocMemberDisplayText final {
  std::vector<std::string> values;
};

// Decodes one complete LOC member.  Text values are returned in their native
// depth-first/tail order.  This is an extraction boundary, not a runtime key
// lookup API; original lookup precedence remains unrecovered.
[[nodiscard]] std::optional<LocMemberDisplayText>
decode_loc_member_display_texts(std::span<const std::byte> bytes);

struct OwnedLocCatalog final {
  // An opaque, text-free identity derived from the ordered normalized logical
  // member IDs and the parser revision.
  std::string source_set;
  std::vector<std::string> values;
};

// Reads every LOC member from a previously verified owned installation.  It
// never writes to the installation and fails closed on malformed framing,
// non-UTF-8 source fields, duplicate logical members, or a partial corpus.
[[nodiscard]] std::optional<OwnedLocCatalog>
extract_verified_owned_loc_catalog(const std::filesystem::path& root);

// Computes the cache identity from verified logical member identities without
// decoding or retaining retail text.  This is the only LOC work on a cache hit.
[[nodiscard]] std::optional<std::string>
verified_owned_loc_source_set(const std::filesystem::path& root);

}  // namespace off::data
