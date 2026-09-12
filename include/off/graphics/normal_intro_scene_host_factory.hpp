#pragma once

#include "off/graphics/movie_control_host_evidence.hpp"
#include "off/graphics/normal_intro_scene_host.hpp"
#include "off/graphics/normal_intro_scene_lifecycle_services.hpp"

namespace off::graphics {

// Construction boundary for a future normal scene.  Evidence proves the
// controller identity and fixed delay; lifecycle services prove that there is
// no fabricated loader/lifecycle fallback.  This factory does not bind normal
// startup, activate the host, or invoke a supplied callback.
class NormalIntroSceneHostFactory final {
 public:
  [[nodiscard]] static NormalIntroSceneHost create(
      const MovieControlHostEvidence& evidence,
      NormalIntroSceneHostLifecycleServices lifecycle,
      NormalIntroSceneHostFirstCutConfig first_cut);
};

}  // namespace off::graphics
