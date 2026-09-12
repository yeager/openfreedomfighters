#pragma once

#include "off/ui/retail_localization_lookup_binding.hpp"

#include <filesystem>
#include <vector>

namespace off::ui::l10n {

// Loads the single, observer-authored lookup artifact from an application
// preferences directory. The record contains opaque site labels and ordinals
// only: it has no retail keys, paths, addresses, or source text. Missing or
// invalid input is optional and therefore yields no admitted artifacts.
[[nodiscard]] std::vector<ReviewedRetailLookupArtifact>
load_local_reviewed_retail_lookup_artifacts(
    const std::filesystem::path &local_directory,
    const TranslationSourceBinding &binding);

} // namespace off::ui::l10n
