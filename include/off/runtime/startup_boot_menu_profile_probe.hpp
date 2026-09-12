#pragma once

#include "off/runtime/startup_boot_menu_component_envelope.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace off::runtime {

// Content-free summary of the already validated BootMenu deferred envelope.
// This is deliberately a reporting boundary: it exposes aggregate framing
// counts and a tag-only digest, never a source directory, handle, byte,
// decoded value, identifier, string, or framing notation.
struct StartupBootMenuProfileProbeSummary final {
  std::size_t deferred_body_bytes{};
  std::array<std::size_t, 6> value_kind_counts{};
  std::size_t attachment_delimiters{};
  std::size_t continuation_values{};
  std::size_t encoded_values{};
  std::uint64_t framing_tag_digest{};

  [[nodiscard]] bool operator==(
      const StartupBootMenuProfileProbeSummary &) const = default;
};

class StartupBootMenuProfileProbe final {
public:
  [[nodiscard]] static StartupBootMenuProfileProbeSummary summarize(
      const StartupBootMenuComponentEnvelope &envelope) {
    if (!envelope.valid())
      throw std::runtime_error("startup BootMenu profile probe requires a valid envelope");
    const auto &profile = envelope.deferred_profile();
    return {.deferred_body_bytes = profile.body_bytes,
            .value_kind_counts = profile.framing.value_kinds,
            .attachment_delimiters = profile.framing.attachment_delimiters,
            .continuation_values = profile.framing.continuation_values,
            .encoded_values = profile.framing.encoded_values,
            .framing_tag_digest = profile.framing.framing_digest};
  }
};

} // namespace off::runtime
