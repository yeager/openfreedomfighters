#pragma once

#include "off/ui/private_translation_pack.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace off::ui::l10n {

class RetailLocalizationSession;

// Source-free reviewed observation output. The site is an observer-assigned
// opaque label, never a retail key, component name, address, path, or text.
struct ReviewedRetailLookupObservation final {
  std::string lookup_site;
  std::uint64_t catalog_ordinal{};
  bool operator==(const ReviewedRetailLookupObservation &) const = default;
};

class ReviewedRetailLookupArtifact final {
public:
  // This type deliberately has no field for raw observations or retail text.
  [[nodiscard]] static std::optional<ReviewedRetailLookupArtifact>
  reviewed(TranslationSourceBinding source,
           std::vector<ReviewedRetailLookupObservation> observations);

private:
  friend class RetailLookupSiteBindings;
  TranslationSourceBinding source_;
  std::vector<ReviewedRetailLookupObservation> observations_;
};

// Maps only reviewed opaque lookup sites to IDs in one enrolled source set.
// Callers cannot provide an opaque translation ID to this interface.
class RetailLookupSiteBindings final {
public:
  [[nodiscard]] static std::optional<RetailLookupSiteBindings>
  admit(const TranslationSourceBinding &source,
        const std::vector<ReviewedRetailLookupArtifact> &artifacts);

private:
  friend class RetailLocalizationSession;
  [[nodiscard]] std::optional<std::string_view>
  id_for_lookup_site(std::string_view lookup_site) const noexcept;
  struct Entry final { std::string lookup_site; std::string id; };
  std::vector<Entry> bindings_;
};

} // namespace off::ui::l10n
