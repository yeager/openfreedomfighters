#pragma once

#include "off/ui/retail_localization_lookup_binding.hpp"
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
       std::string_view source_set, const Extractor &extract,
       const std::vector<ReviewedRetailLookupArtifact> &lookup_artifacts = {});

  [[nodiscard]] const RetailLocalizationMetadata &metadata() const noexcept {
    return metadata_;
  }
  [[nodiscard]] const TranslationSourceBinding &binding() const noexcept {
    return binding_;
  }
  [[nodiscard]] bool has_local_resolver() const noexcept {
    return resolver_.has_value();
  }
  // This exposes only enrollment state, never the private source strings.
  // `loaded` means an identity-matching private snapshot was reused;
  // `extracted` means this process admitted and stored a fresh snapshot.
  [[nodiscard]] RetailLocalizationCacheStatus cache_status() const noexcept {
    return cache_status_;
  }
  [[nodiscard]] std::size_t local_pack_count() const noexcept {
    return local_pack_count_;
  }
  [[nodiscard]] bool has_private_fallback() const noexcept {
    return retail_fallback_.has_value();
  }

  // Resolution is scoped to an admitted opaque lookup site. No caller can
  // submit an opaque ID or resolve an unobserved site.
  [[nodiscard]] std::optional<std::string_view>
  resolve_lookup_site(std::string_view lookup_site, std::string_view explicit_locale,
                    std::span<const std::string_view> platform_locales) const
      noexcept;

private:
  RetailLocalizationMetadata metadata_;
  TranslationSourceBinding binding_;
  RetailLocalizationCacheStatus cache_status_{
      RetailLocalizationCacheStatus::unavailable};
  std::size_t local_pack_count_{};
  std::optional<PrivateTranslationResolver> resolver_;
  std::optional<RetailTranslationCatalog> retail_fallback_;
  std::optional<RetailLookupSiteBindings> lookup_bindings_;
};

} // namespace off::ui::l10n
