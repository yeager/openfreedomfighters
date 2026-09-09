#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_locale.h>

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
  std::vector<std::string> result;
  result.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    const auto *locale = locales[index];
    if (locale == nullptr || locale->language == nullptr || locale->language[0] == '\0')
      continue;
    std::string tag{locale->language};
    if (locale->country != nullptr && locale->country[0] != '\0') {
      tag += '-';
      tag += locale->country;
    }
    result.push_back(std::move(tag));
  }
  SDL_free(locales);
  return result;
}

[[nodiscard]] inline std::string preferred_system_locale() {
  const auto locales = preferred_system_locales();
  return locales.empty() ? std::string{} : locales.front();
}

} // namespace off::platform
