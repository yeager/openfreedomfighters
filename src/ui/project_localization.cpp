#include "off/ui/project_localization.hpp"

#include <array>
#include <stdexcept>

namespace off::ui::l10n {
namespace {

bool valid_utf8(std::string_view value) noexcept {
  for (std::size_t i = 0; i < value.size();) {
    const auto lead = static_cast<unsigned char>(value[i]);
    if (lead <= 0x7fU) {
      ++i;
      continue;
    }
    unsigned continuation_count = 0;
    std::uint32_t code_point = 0;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      continuation_count = 1;
      code_point = lead & 0x1fU;
    } else if (lead >= 0xe0U && lead <= 0xefU) {
      continuation_count = 2;
      code_point = lead & 0x0fU;
    } else if (lead >= 0xf0U && lead <= 0xf4U) {
      continuation_count = 3;
      code_point = lead & 0x07U;
    } else {
      return false;
    }
    if (i + continuation_count >= value.size())
      return false;
    for (unsigned j = 1; j <= continuation_count; ++j) {
      const auto byte = static_cast<unsigned char>(value[i + j]);
      if ((byte & 0xc0U) != 0x80U)
        return false;
      code_point = (code_point << 6U) | (byte & 0x3fU);
    }
    const auto minimum = continuation_count == 1   ? 0x80U
                         : continuation_count == 2 ? 0x800U
                                                   : 0x10000U;
    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU))
      return false;
    i += continuation_count + 1;
  }
  return true;
}

std::optional<Locale> locale_from_tag(std::string_view tag) noexcept {
  const auto language_end = tag.find_first_of("-_.");
  const auto language = tag.substr(0, language_end);
  if (language.size() == 2 && (language[0] == 'e' || language[0] == 'E') &&
      (language[1] == 'n' || language[1] == 'N'))
    return Locale::english;
  if (language.size() == 2 && (language[0] == 's' || language[0] == 'S') &&
      (language[1] == 'v' || language[1] == 'V'))
    return Locale::swedish;
  return std::nullopt;
}

std::optional<Locale> select_locale(std::string_view explicit_locale,
                                    std::string_view platform_locale) noexcept {
  if (const auto locale = locale_from_tag(explicit_locale))
    return locale;
  if (const auto locale = locale_from_tag(platform_locale))
    return locale;
  return Locale::english;
}

constexpr std::array<CatalogEntry, message_id_count * locale_count> f10_entries{
    {
        {Locale::english, MessageId::graphics_settings, "GRAPHICS SETTINGS"},
        {Locale::english, MessageId::profile, "Profile"},
        {Locale::english, MessageId::window_mode, "Window mode"},
        {Locale::english, MessageId::resolution, "Resolution"},
        {Locale::english, MessageId::present_mode, "Present mode"},
        {Locale::english, MessageId::render_scale, "Render scale"},
        {Locale::english, MessageId::upscaler, "Upscaler"},
        {Locale::english, MessageId::shadows, "Shadows"},
        {Locale::english, MessageId::apply, "Apply"},
        {Locale::english, MessageId::back, "Back"},
        {Locale::english, MessageId::defaults, "Defaults"},
        {Locale::english, MessageId::keep, "Keep"},
        {Locale::english, MessageId::revert, "Revert"},
        {Locale::english, MessageId::applying_settings, "Applying settings..."},
        {Locale::english, MessageId::restoring_settings,
         "Restoring settings..."},
        {Locale::english, MessageId::keep_display_settings,
         "Keep these display settings?"},
        {Locale::english, MessageId::reverting_in_seconds,
         "Reverting in {seconds} seconds"},
        {Locale::english, MessageId::original, "Original"},
        {Locale::english, MessageId::modern, "Modern"},
        {Locale::english, MessageId::modern_plus, "Modern+"},
        {Locale::english, MessageId::windowed, "Windowed"},
        {Locale::english, MessageId::borderless_desktop, "Borderless desktop"},
        {Locale::english, MessageId::vsync, "VSync"},
        {Locale::english, MessageId::mailbox, "Mailbox"},
        {Locale::english, MessageId::immediate, "Immediate"},
        {Locale::english, MessageId::native, "Native"},
        {Locale::english, MessageId::temporal, "Temporal"},
        {Locale::english, MessageId::dlss, "DLSS"},
        {Locale::english, MessageId::reference, "Reference"},
        {Locale::english, MessageId::high, "High"},
        {Locale::english, MessageId::ultra, "Ultra"},
        {Locale::swedish, MessageId::graphics_settings, "GRAFIKINSTÄLLNINGAR"},
        {Locale::swedish, MessageId::profile, "Profil"},
        {Locale::swedish, MessageId::window_mode, "Fönsterläge"},
        {Locale::swedish, MessageId::resolution, "Upplösning"},
        {Locale::swedish, MessageId::present_mode, "Presentationsläge"},
        {Locale::swedish, MessageId::render_scale, "Renderingsskala"},
        {Locale::swedish, MessageId::upscaler, "Uppskalning"},
        {Locale::swedish, MessageId::shadows, "Skuggor"},
        {Locale::swedish, MessageId::apply, "Tillämpa"},
        {Locale::swedish, MessageId::back, "Tillbaka"},
        {Locale::swedish, MessageId::defaults, "Standardvärden"},
        {Locale::swedish, MessageId::keep, "Behåll"},
        {Locale::swedish, MessageId::revert, "Återställ"},
        {Locale::swedish, MessageId::applying_settings,
         "Tillämpar inställningar..."},
        {Locale::swedish, MessageId::restoring_settings,
         "Återställer inställningar..."},
        {Locale::swedish, MessageId::keep_display_settings,
         "Behålla dessa bildskärmsinställningar?"},
        {Locale::swedish, MessageId::reverting_in_seconds,
         "Återställer om {seconds} sekunder"},
        {Locale::swedish, MessageId::original, "Original"},
        {Locale::swedish, MessageId::modern, "Modern"},
        {Locale::swedish, MessageId::modern_plus, "Modern+"},
        {Locale::swedish, MessageId::windowed, "Fönster"},
        {Locale::swedish, MessageId::borderless_desktop, "Kantlöst skrivbord"},
        {Locale::swedish, MessageId::vsync, "VSync"},
        {Locale::swedish, MessageId::mailbox, "Mailbox"},
        {Locale::swedish, MessageId::immediate, "Omedelbar"},
        {Locale::swedish, MessageId::native, "Inbyggd"},
        {Locale::swedish, MessageId::temporal, "Temporal"},
        {Locale::swedish, MessageId::dlss, "DLSS"},
        {Locale::swedish, MessageId::reference, "Referens"},
        {Locale::swedish, MessageId::high, "Hög"},
        {Locale::swedish, MessageId::ultra, "Ultra"},
    }};

} // namespace

CatalogBuildResult
ProjectCatalog::build(std::span<const CatalogEntry> entries) {
  ProjectCatalog catalog;
  std::array<std::array<bool, message_id_count>, locale_count> seen{};
  for (const auto &entry : entries) {
    const auto locale = static_cast<std::size_t>(entry.locale);
    const auto id = static_cast<std::size_t>(entry.id);
    if (locale >= locale_count)
      return {.catalog = std::nullopt, .error = CatalogError::invalid_locale};
    if (id >= message_id_count)
      return {.catalog = std::nullopt,
              .error = CatalogError::invalid_message_id};
    if (entry.text.empty())
      return {.catalog = std::nullopt, .error = CatalogError::empty_message};
    if (!valid_utf8(entry.text))
      return {.catalog = std::nullopt, .error = CatalogError::invalid_utf8};
    if (seen[locale][id])
      return {.catalog = std::nullopt,
              .error = CatalogError::duplicate_message};
    seen[locale][id] = true;
    catalog.messages_[locale][id] = entry.text;
  }
  for (const auto &by_locale : seen)
    for (const auto present : by_locale)
      if (!present)
        return {.catalog = std::nullopt,
                .error = CatalogError::missing_message};
  return {.catalog = std::move(catalog), .error = std::nullopt};
}

std::optional<std::string_view>
ProjectCatalog::resolve(MessageId id, std::string_view explicit_locale,
                        std::string_view platform_locale) const noexcept {
  const auto index = static_cast<std::size_t>(id);
  if (index >= message_id_count)
    return std::nullopt;
  const auto locale = *select_locale(explicit_locale, platform_locale);
  return messages_[static_cast<std::size_t>(locale)][index];
}

std::optional<std::string_view>
ProjectCatalog::format_seconds(MessageId id, unsigned seconds,
                               std::string_view explicit_locale,
                               std::string_view platform_locale) const {
  const auto pattern = resolve(id, explicit_locale, platform_locale);
  if (!pattern || id != MessageId::reverting_in_seconds)
    return std::nullopt;
  const auto marker = pattern->find("{seconds}");
  if (marker == std::string_view::npos)
    return std::nullopt;
  thread_local std::string formatted;
  formatted = std::string{pattern->substr(0, marker)} +
              std::to_string(seconds) +
              std::string{pattern->substr(marker + 9)};
  return formatted;
}

const ProjectCatalog &f10_catalog() {
  static const ProjectCatalog catalog = [] {
    const auto result = ProjectCatalog::build(f10_entries);
    if (!result.catalog)
      throw std::logic_error("built-in F10 catalog is invalid");
    return *result.catalog;
  }();
  return catalog;
}

} // namespace off::ui::l10n
