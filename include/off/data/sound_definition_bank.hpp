#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace off::data {

struct SimpleSoundDefinition {
  std::uint32_t definition_offset;
  std::uint32_t identifier_offset;
  // Distinct opaque resource identity; not an identifier offset or WHD row.
  std::uint32_t resource_link;
  std::uint32_t duration_bits;
  float duration;
  // Original logical name bytes, not a validated filesystem path or UTF-8 text.
  std::string logical_identifier;
};

// Owns the complete unwrapped SND image.  The four-word member envelope is
// corpus-validated before any interior definition reference can be accepted:
// its declared payload size is member bytes minus 48, its declared member size
// is exact, and the remaining two words are the observed 3/4 revision pair.
// Record validation still happens on lookup. Successful parsing does not prove
// every definition variant valid or authorize playback.
class SoundDefinitionBank final {
public:
  [[nodiscard]] static SoundDefinitionBank parse(
      std::span<const std::byte> image, std::size_t byte_budget);
  [[nodiscard]] std::size_t size() const noexcept { return image_.size(); }
  // Zero is null. Nonzero offsets are relative to the complete image base.
  // Only type 1 is supported. Identifier limit excludes its terminating zero.
  // Returned data owns its name and survives destruction of this bank.
  // Finite nonnegative duration is a native policy, not a playback/readiness test.
  [[nodiscard]] std::optional<SimpleSoundDefinition> simple_definition(
      std::uint32_t reference, std::size_t identifier_byte_limit = 4096) const;
private:
  std::vector<std::byte> image_;
};

} // namespace off::data
