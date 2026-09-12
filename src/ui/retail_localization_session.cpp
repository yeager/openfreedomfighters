#include "off/ui/retail_localization_session.hpp"

#include "retail_localization_cache_private.hpp"

namespace off::ui::l10n {

std::optional<RetailLocalizationSession> RetailLocalizationSession::open(
    const std::filesystem::path &cache_root,
    const std::filesystem::path &local_packs_directory,
    std::string_view installation_identity, std::string_view parser_identity,
    std::string_view source_set, const Extractor &extract) {
  const auto enrollment = ensure_retail_localization_metadata(
      cache_root, installation_identity, parser_identity, source_set, extract);
  if (!enrollment.metadata)
    return std::nullopt;
  const auto binding = translation_source_binding(*enrollment.metadata);
  if (!binding)
    return std::nullopt;

  RetailLocalizationSession result;
  result.metadata_ = std::move(*enrollment.metadata);
  result.binding_ = *binding;
  result.retail_fallback_ =
      detail::load_private_retail_source_fallback(cache_root, result.metadata_);
  const auto packs =
      load_canonical_local_translation_packs(local_packs_directory, *binding);
  if (!packs.empty())
    result.resolver_ = PrivateTranslationResolver::build(*binding, packs);
  return result;
}

std::optional<std::string_view> RetailLocalizationSession::resolve_opaque_id(
    std::string_view id, std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales) const noexcept {
  if (!resolver_)
    return retail_fallback_ ? retail_fallback_->find(id) : std::nullopt;
  if (const auto translated =
          resolver_->resolve(id, explicit_locale, platform_locales))
    return translated;
  return retail_fallback_ ? retail_fallback_->find(id) : std::nullopt;
}

} // namespace off::ui::l10n
