#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::platform {

// Converts a host locale spelling to a small, ASCII BCP-47-like tag. This is
// deliberately a host-input boundary: malformed values are ignored rather
// than allowed to affect the project's deterministic fallback order.
[[nodiscard]] std::optional<std::string>
canonical_host_locale_tag(std::string_view value) noexcept;

// Preserves the host preference order, omits malformed entries, and removes
// canonical duplicates. The result is suitable for ProjectCatalog and private
// translation-pack fallback without granting a malformed first entry priority.
[[nodiscard]] std::vector<std::string>
canonical_host_locale_preferences(std::span<const std::string_view> values);

} // namespace off::platform
