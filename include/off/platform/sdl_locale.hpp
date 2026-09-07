#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_locale.h>

#include <string>

namespace off::platform {

// SDL owns the returned array. The first preferred locale is the platform's
// primary UI language; callers may supply an explicit user choice separately.
[[nodiscard]] inline std::string preferred_system_locale() {
  int count = 0;
  SDL_Locale **locales = SDL_GetPreferredLocales(&count);
  if (locales == nullptr || count <= 0 || locales[0] == nullptr ||
      locales[0]->language == nullptr) {
    SDL_free(locales);
    return {};
  }
  std::string result{locales[0]->language};
  if (locales[0]->country != nullptr && locales[0]->country[0] != '\0') {
    result += '-';
    result += locales[0]->country;
  }
  SDL_free(locales);
  return result;
}

} // namespace off::platform
