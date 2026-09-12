#pragma once

#include "off/ui/retail_localization_cache.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::ui::l10n {

// This is deliberately text-free. It is derived from validated private
// extraction metadata, then used to bind a local translation pack to the
// exact parser, source-set and canonical ordinal span that produced it.
struct TranslationSourceBinding final {
  std::string parser_identity;
  std::string source_set;
  std::uint64_t first_ordinal{};
  std::uint64_t ordinal_count{};
  bool operator==(const TranslationSourceBinding &) const = default;
};

[[nodiscard]] std::optional<TranslationSourceBinding>
translation_source_binding(const RetailLocalizationMetadata &metadata);

struct PrivateTranslationEntry final {
  std::string id;
  std::string text;
};

// A pack has no source-text field. It is loaded only from a regular,
// non-symlink file directly below a caller-selected local directory.
class PrivateTranslationPack final {
public:
  [[nodiscard]] static std::optional<PrivateTranslationPack>
  load_local(const std::filesystem::path &local_directory,
             std::string_view filename,
             const TranslationSourceBinding &binding);

  [[nodiscard]] std::string_view locale() const noexcept { return locale_; }
  [[nodiscard]] bool declared_complete() const noexcept { return complete_; }
  [[nodiscard]] const TranslationSourceBinding &binding() const noexcept {
    return binding_;
  }
  [[nodiscard]] std::optional<std::string_view>
  find(std::string_view id) const noexcept;

private:
  friend class PrivateTranslationResolver;
  [[nodiscard]] static std::optional<PrivateTranslationPack>
  decode(std::string_view bytes, const TranslationSourceBinding &binding);
  TranslationSourceBinding binding_;
  std::string locale_;
  bool complete_{};
  std::vector<PrivateTranslationEntry> entries_;
};

// Explicit locale wins, followed by the platform preference list and English.
// If a partial preferred pack has no requested ID, resolution continues to the
// next preference. No retail lookup or source string enters this API.
class PrivateTranslationResolver final {
public:
  [[nodiscard]] static std::optional<PrivateTranslationResolver>
  build(TranslationSourceBinding binding,
        std::vector<PrivateTranslationPack> packs);

  [[nodiscard]] std::optional<std::string_view>
  resolve(std::string_view id, std::string_view explicit_locale,
          std::span<const std::string_view> platform_locales) const noexcept;

private:
  TranslationSourceBinding binding_;
  std::vector<PrivateTranslationPack> packs_;
};

} // namespace off::ui::l10n
