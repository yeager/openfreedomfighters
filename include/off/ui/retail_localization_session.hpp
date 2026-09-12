#pragma once

#include "off/ui/private_translation_pack.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace off::ui::l10n {

// A bounded, per-process owner for the private localization enrollment
// substrate. It stores text-free extraction metadata, independently authored
// local translation packs, and a private fallback loaded by the UI
// implementation from the cache. It neither enumerates game data nor maps
// any retail component/key to an opaque translation ID.
class RetailLocalizationSession final {
public:
  using Extractor =
      std::function<std::optional<std::vector<RetailSourceString>>()>;

  // Opens a session only after the private cache has accepted the supplied
  // recovered extractor. Local packs remain optional: a successful session
  // may deliberately have no resolver.
  [[nodiscard]] static std::optional<RetailLocalizationSession>
  open(const std::filesystem::path &cache_root,
       const std::filesystem::path &local_packs_directory,
       std::string_view installation_identity, std::string_view parser_identity,
       std::string_view source_set, const Extractor &extract);

  [[nodiscard]] const RetailLocalizationMetadata &metadata() const noexcept {
    return metadata_;
  }
  [[nodiscard]] const TranslationSourceBinding &binding() const noexcept {
    return binding_;
  }
  [[nodiscard]] bool has_local_resolver() const noexcept {
    return resolver_.has_value();
  }

  // The caller must already possess an approved opaque ID. This function does
  // not inspect retail files, source text, or component state.
  [[nodiscard]] std::optional<std::string_view>
  resolve_opaque_id(std::string_view id, std::string_view explicit_locale,
                    std::span<const std::string_view> platform_locales) const
      noexcept;

private:
  RetailLocalizationMetadata metadata_;
  TranslationSourceBinding binding_;
  std::optional<PrivateTranslationResolver> resolver_;
  std::optional<RetailTranslationCatalog> retail_fallback_;
};

} // namespace off::ui::l10n
