#pragma once

#include "off/mode.hpp"

namespace off::platform {

// The scene path always has a portable trilinear sampler.  Modern may select
// the separately-created anisotropic sampler only when the active SDL GPU
// device accepted it.  This is a presentation choice; it cannot affect scene
// admission, transforms, simulation, or any vendor upscaler capability.
enum class SceneTextureFiltering { trilinear, anisotropic_requested };

[[nodiscard]] constexpr SceneTextureFiltering
select_scene_texture_filtering(Mode profile,
                               bool anisotropic_sampler_available) noexcept {
  return profile == Mode::modern && anisotropic_sampler_available
             ? SceneTextureFiltering::anisotropic_requested
             : SceneTextureFiltering::trilinear;
}

} // namespace off::platform
