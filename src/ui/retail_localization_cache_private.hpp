#pragma once

#include "off/ui/retail_localization_cache.hpp"

#include <filesystem>
#include <optional>

namespace off::ui::l10n::detail {

// Internal-only bridge from a private cache snapshot to the runtime session.
// It is intentionally not installed under include/: callers outside the UI
// implementation cannot enumerate or export cached retail text through it.
[[nodiscard]] std::optional<RetailTranslationCatalog>
load_private_retail_source_fallback(
    const std::filesystem::path &cache_root,
    const RetailLocalizationMetadata &metadata);

} // namespace off::ui::l10n::detail
