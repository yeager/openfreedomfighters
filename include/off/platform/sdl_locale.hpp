#pragma once

#include "off/platform/locale_preferences.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_locale.h>

#include <span>
#include <string>
#include <vector>

namespace off::platform {

// SDL owns the returned array. Preserve its preference order so callers can
// choose the first locale for which the application has a catalog.
[[nodiscard]] inline std::vector<std::string> preferred_system_locales() {
  int count = 0;
  SDL_Locale **locales = SDL_GetPreferredLocales(&count);
  if (locales == nullptr || count <= 0) {
    SDL_free(locales);
    return {};
  }
  std::vector<std::string> raw;
  raw.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    const auto *locale = locales[index];
    if (locale == nullptr || locale->language == nullptr || locale->language[0] == '\0')
      continue;
    std::string tag{locale->language};
    if (locale->country != nullptr && locale->country[0] != '\0') {
      tag += '-';
      tag += locale->country;
    }
    raw.push_back(std::move(tag));
  }
  SDL_free(locales);
  std::vector<std::string_view> views;
  views.reserve(raw.size());
  for (const auto &locale : raw)
    views.push_back(locale);
  return canonical_host_locale_preferences(views);
}

[[nodiscard]] inline std::string preferred_system_locale() {
  const auto locales = preferred_system_locales();
  return locales.empty() ? std::string{} : locales.front();
}

} // namespace off::platform
