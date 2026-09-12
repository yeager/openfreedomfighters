#include "off/graphics/movie_control_first_cut_scene_frame_adapter.hpp"

#include <stdexcept>
#include <utility>

namespace off::graphics {

MovieControlFirstCutSceneFrameAdapter MovieControlFirstCutSceneFrameAdapter::bind(
    MovieControlFirstUpdate& movie_control,
    MovieControlFirstCutRuntimeHandoff handoff) {
  return MovieControlFirstCutSceneFrameAdapter(movie_control, std::move(handoff));
}

MovieControlEvent16Result MovieControlFirstCutSceneFrameAdapter::run_frame(
    const MovieControlFirstCutSceneFrameInputs& inputs,
    MovieControlFirstCutSceneFrameServices services) {
  if (!movie_control_)
    throw std::runtime_error("MovieControl first-cut scene-frame adapter is unbound");
  if (delivered_)
    throw std::runtime_error("MovieControl first-cut scene-frame adapter is exhausted");
  if (inputs.component_handle != movie_control_->component_handle())
    throw std::runtime_error("MovieControl first-cut scene-frame component does not match handoff");

  const auto snapshot = MovieControlEvent16ManagerSnapshot::capture(
      inputs.manager, inputs.component, inputs.component_handle,
      inputs.scene_integer, inputs.paused, inputs.component_filter);
  const auto result = handoff_.deliver(*movie_control_, snapshot.bind(std::move(services.direct)),
                                       services.phase_one, services.phase_two);
  if (result == MovieControlEvent16Result::activated)
    delivered_ = true;
  return result;
}

}  // namespace off::graphics
