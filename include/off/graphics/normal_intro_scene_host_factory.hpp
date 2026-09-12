#pragma once

#include "off/graphics/movie_control_host_evidence.hpp"
#include "off/graphics/normal_intro_scene_host.hpp"

#include <functional>

namespace off::graphics {

// These are the three already-separated startup boundaries that the host will
// use only when it is explicitly activated later.  The factory validates their
// presence but never invokes them: construction must not advance a scene.
struct NormalIntroSceneHostLifecycleServices {
  std::function<void()> reader_bracket;
  std::function<void()> outer_loader_tail;
  std::function<void()> enter_global_lifecycle;
};

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
