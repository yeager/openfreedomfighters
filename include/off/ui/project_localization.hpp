#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace off::ui::l10n {

// These identifiers name only OpenFreedomFighters-authored F10 UI text. They
// deliberately do not identify, parse, or replace any retail text resource.
enum class MessageId : std::size_t {
  graphics_settings,
  profile,
  window_mode,
  resolution,
  present_mode,
  render_scale,
  upscaler,
  shadows,
  apply,
  back,
  defaults,
  keep,
  revert,
  applying_settings,
  restoring_settings,
  keep_display_settings,
  reverting_in_seconds,
  game_data_required,
  game_data_required_intro,
  game_data_folder_unavailable,
  game_executable_missing,
  game_executable_unsupported,
  game_data_incomplete,
  game_data_unreadable,
  game_data_verification_failed,
  relaunch_with_data_path,
  technical_details,
  verifying_game_data,
  preparing_startup,
  original,
  modern,
  modern_plus,
  windowed,
  borderless_desktop,
  vsync,
  mailbox,
  immediate,
  native,
  temporal,
  dlss,
  reference,
  high,
  ultra,
  count
};

inline constexpr std::size_t message_id_count =
    static_cast<std::size_t>(MessageId::count);

// The supported values are intentionally language-level preferences. Region
// subtags are accepted by the resolver but do not select a separate catalog.
enum class Locale : std::size_t {
  english,
  swedish,
  danish,
  norwegian_bokmal,
  finnish,
  german,
  french,
  spanish,
  italian,
  portuguese_brazil,
  polish,
  czech,
  hungarian,
  romanian,
  turkish,
  russian,
  ukrainian,
  japanese,
  korean,
  simplified_chinese,
  count
};
inline constexpr std::size_t locale_count =
    static_cast<std::size_t>(Locale::count);

struct CatalogEntry {
  Locale locale{};
  MessageId id{};
  std::string_view text;
};

enum class CatalogError : std::uint8_t {
  invalid_locale,
  invalid_message_id,
  empty_message,
  invalid_utf8,
  duplicate_message,
  missing_message,
};

struct CatalogBuildResult;

class ProjectCatalog final {
public:
  [[nodiscard]] static CatalogBuildResult
  build(std::span<const CatalogEntry> entries);

  // Locale preference is intentionally explicit. An unavailable explicit
  // locale falls through to the platform locale, then to English.
  [[nodiscard]] std::optional<std::string_view>
  resolve(MessageId id, std::string_view explicit_locale,
          std::string_view platform_locale) const noexcept;
  // The ordered platform preference list is consulted only after an explicit
  // override. This permits a supported secondary system language to win over
  // English when the primary preference has no project catalog.
  [[nodiscard]] std::optional<std::string_view>
  resolve(MessageId id, std::string_view explicit_locale,
          std::span<const std::string_view> platform_locales) const noexcept;
  [[nodiscard]] std::optional<std::string_view>
  format_seconds(MessageId id, unsigned seconds,
                 std::string_view explicit_locale,
                 std::string_view platform_locale) const;
  [[nodiscard]] std::optional<std::string_view>
  format_seconds(MessageId id, unsigned seconds,
                 std::string_view explicit_locale,
                 std::span<const std::string_view> platform_locales) const;

private:
  friend struct CatalogBuildResult;
  std::array<std::array<std::string, message_id_count>, locale_count>
      messages_{};
};

struct CatalogBuildResult {
  std::optional<ProjectCatalog> catalog;
  std::optional<CatalogError> error;
};

// An independently authored catalog for OpenFreedomFighters UI, including the
// F10 menu and startup status. It is not a declaration that either locale has
// complete game coverage.
[[nodiscard]] const ProjectCatalog &f10_catalog();

} // namespace off::ui::l10n
