#pragma once

#include "off/cutscene/first_cut_player_initialization.hpp"
#include "off/graphics/intro_controller_initialization.hpp"

namespace off::graphics {

class IntroRuntime;

// The sole native bridge from the admitted MovieControl update to the already
// prepared first-cut player.  It is intentionally narrower than a scene event
// dispatcher: no target lookup, clock, asset, text, rendering, or playback
// service is accepted here.  Construction verifies the currently-live runtime
// relations; a runtime which cannot prove every relation has no bridge.
class MovieControlFirstCutRuntimeHandoff final {
 public:
  [[nodiscard]] static MovieControlFirstCutRuntimeHandoff from_runtime(
      IntroRuntime& runtime, cutscene::FirstCutPlayerSession& player);

  MovieControlFirstCutRuntimeHandoff(const MovieControlFirstCutRuntimeHandoff&) = delete;
  MovieControlFirstCutRuntimeHandoff& operator=(const MovieControlFirstCutRuntimeHandoff&) = delete;
  MovieControlFirstCutRuntimeHandoff(MovieControlFirstCutRuntimeHandoff&&) = default;
  MovieControlFirstCutRuntimeHandoff& operator=(MovieControlFirstCutRuntimeHandoff&&) = default;

  // Delivery enters through MovieControl's own event-16 admission.  On an
  // activated event, this replaces only its CutSequence-start receiver with
  // the two existing player lifecycle phases and invokes them synchronously.
  // Waiting/skipped events leave the cold player untouched.  This does not
  // claim that a cut, view, frame, or visual playback completed.
  [[nodiscard]] MovieControlEvent16Result deliver(
      MovieControlFirstUpdate& movie_control, MovieControlEvent16Services event16,
      const cutscene::FirstCutPlayerPhaseOneServices& phase_one,
      const cutscene::FirstCutPlayerSessionPhaseTwoServices& phase_two);

 private:
  MovieControlFirstCutRuntimeHandoff(IntroRuntime& runtime,
                                     cutscene::FirstCutPlayerSession& player)
      : runtime_(&runtime), player_(&player) {}

  void require_live_relations() const;
  IntroRuntime* runtime_{};
  cutscene::FirstCutPlayerSession* player_{};
  bool delivered_{};
};

}  // namespace off::graphics
