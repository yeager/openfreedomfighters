#include "off/ui/retail_localization_lookup_binding.hpp"

#include <algorithm>
#include <ranges>

namespace off::ui::l10n {
namespace {
bool valid_site(std::string_view value) noexcept {
  return !value.empty() && value.size() <= 128U && std::ranges::all_of(value, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-';
  });
}
bool valid_source(const TranslationSourceBinding &source) noexcept {
  return !source.parser_identity.empty() && !source.source_set.empty() &&
         source.first_ordinal == 0U && source.ordinal_count != 0U;
}
} // namespace

std::optional<ReviewedRetailLookupArtifact> ReviewedRetailLookupArtifact::reviewed(
    TranslationSourceBinding source,
    std::vector<ReviewedRetailLookupObservation> observations) {
  if (!valid_source(source) || observations.empty()) return std::nullopt;
  for (const auto &observation : observations)
    if (!valid_site(observation.lookup_site)) return std::nullopt;
  ReviewedRetailLookupArtifact result;
  result.source_ = std::move(source);
  result.observations_ = std::move(observations);
  return result;
}

std::optional<RetailLookupSiteBindings> RetailLookupSiteBindings::admit(
    const TranslationSourceBinding &source,
    const std::vector<ReviewedRetailLookupArtifact> &artifacts) {
  if (!valid_source(source) || artifacts.empty()) return std::nullopt;
  RetailLookupSiteBindings result;
  for (const auto &artifact : artifacts) {
    if (artifact.source_ != source || artifact.observations_.empty()) return std::nullopt;
    for (const auto &observation : artifact.observations_) {
      if (!valid_site(observation.lookup_site) ||
          observation.catalog_ordinal < source.first_ordinal ||
          observation.catalog_ordinal - source.first_ordinal >= source.ordinal_count)
        return std::nullopt;
      result.bindings_.push_back({observation.lookup_site,
          make_retail_string_id(source.source_set, observation.catalog_ordinal)});
    }
  }
  std::ranges::sort(result.bindings_, {}, &Entry::lookup_site);
  if (std::ranges::adjacent_find(result.bindings_, {}, &Entry::lookup_site) !=
      result.bindings_.end()) return std::nullopt;
  return result.bindings_.empty() ? std::nullopt : std::optional<RetailLookupSiteBindings>{std::move(result)};
}

std::optional<std::string_view> RetailLookupSiteBindings::id_for_lookup_site(
    std::string_view lookup_site) const noexcept {
  const auto found = std::ranges::lower_bound(bindings_, lookup_site, {}, &Entry::lookup_site);
  if (found == bindings_.end() || found->lookup_site != lookup_site) return std::nullopt;
  return found->id;
}
} // namespace off::ui::l10n
