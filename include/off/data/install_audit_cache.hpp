#pragma once

#include <filesystem>
#include <string_view>

namespace off::data {

// A successful record is only a performance hint. `identity` must be a
// lowercase 64-character SHA-256 value. Invalid, inaccessible, or hostile
// cache entries are treated as misses and never affect verification.
[[nodiscard]] bool install_audit_cache_hit(const std::filesystem::path& root,
                                           std::string_view identity) noexcept;
void store_install_audit_cache(const std::filesystem::path& root,
                               std::string_view identity) noexcept;

} // namespace off::data
