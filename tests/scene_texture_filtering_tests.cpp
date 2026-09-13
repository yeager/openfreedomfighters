#include "off/platform/scene_texture_filtering.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  using off::Mode;
  using off::platform::SceneTextureFiltering;
  using off::platform::select_scene_texture_filtering;

  check(select_scene_texture_filtering(Mode::original, true) ==
            SceneTextureFiltering::trilinear,
        "Original never enables the Modern anisotropic sampler");
  check(select_scene_texture_filtering(Mode::modern, false) ==
            SceneTextureFiltering::trilinear,
        "Modern falls back to the portable sampler when anisotropy is unavailable");
  check(select_scene_texture_filtering(Mode::modern, true) ==
            SceneTextureFiltering::anisotropic_requested,
        "Modern selects the accepted anisotropic sampler request");
  return failures == 0 ? 0 : 1;
}
