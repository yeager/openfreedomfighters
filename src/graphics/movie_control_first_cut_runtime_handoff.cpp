#include "off/graphics/movie_control_first_cut_runtime_handoff.hpp"

#include "off/graphics/intro_runtime.hpp"

#include <stdexcept>

namespace off::graphics {
namespace {
void require_live_component(const IntroRuntime& runtime, std::size_t index,
                            std::uint64_t owner, std::string_view factory) {
  if (index >= runtime.components().size())
    throw std::runtime_error("MovieControl first-cut handoff component is absent");
  const auto& component = runtime.components().at(index);
  if (!component.constructed() || component.removed() ||
      component.source().factory_name != factory ||
      component.state().attached_owner != owner)
    throw std::runtime_error("MovieControl first-cut handoff component/owner relation is not live");
}
}  // namespace

MovieControlFirstCutRuntimeHandoff MovieControlFirstCutRuntimeHandoff::from_runtime(
    IntroRuntime& runtime, cutscene::FirstCutPlayerSession& player) {
  MovieControlFirstCutRuntimeHandoff result(runtime, player);
  result.require_live_relations();
  if (player.initialization().phase_one_complete() ||
      player.initialization().phase_two_complete() || player.receiver().open() ||
      player.receiver().closed())
    throw std::runtime_error("MovieControl first-cut handoff requires a cold player lifecycle");
  return result;
}

void MovieControlFirstCutRuntimeHandoff::require_live_relations() const {
  if (!runtime_ || !player_)
    throw std::runtime_error("MovieControl first-cut handoff is unbound");
  const auto* movie = runtime_->movie_controller_reader_state();
  const auto* movie_component = runtime_->movie_controller_component_reader_state();
  const auto* sequence = runtime_->first_cut_sequence_reader_state();
  const auto* sequence_component = runtime_->first_cut_sequence_component_reader_state();
  if (!movie || !movie_component || !sequence || !sequence_component ||
      !runtime_->first_cut_player_prepared_state())
    throw std::runtime_error("MovieControl first-cut handoff lacks reviewed runtime state");
  const auto expected_player = runtime_->first_cut_player_initialization();
  if (movie_component->owner != movie->owner || movie_component->resource != movie->resource ||
      movie_component->component_index != movie->component_index ||
      sequence_component->owner != sequence->owner ||
      sequence_component->resource != sequence->resource ||
      sequence_component->component_index != sequence->component_index ||
      player_->initialization().list_component() != expected_player.list_component())
    throw std::runtime_error("MovieControl first-cut handoff reader relations changed");
  if (!player_->initialization().has_same_descriptor_as(expected_player))
    throw std::runtime_error("MovieControl first-cut handoff player descriptor changed");
  require_live_component(*runtime_, movie->component_index, movie->owner.value,
                         "ZGEOM_MovieControl");
  require_live_component(*runtime_, sequence->component_index, sequence->owner.value,
                         "ZLIST_CutSequence");
}

MovieControlEvent16Result MovieControlFirstCutRuntimeHandoff::deliver(
    MovieControlFirstUpdate& movie_control, MovieControlEvent16Services event16,
    const cutscene::FirstCutPlayerPhaseOneServices& phase_one,
    const cutscene::FirstCutPlayerSessionPhaseTwoServices& phase_two) {
  if (delivered_)
    throw std::runtime_error("MovieControl first-cut handoff is once-only");
  require_live_relations();
  if (player_->initialization().phase_one_complete() || player_->initialization().phase_two_complete())
    throw std::runtime_error("MovieControl first-cut handoff player is no longer cold");
  // This is deliberately a typed lifecycle receiver, not a generic event
  // dispatch. MovieControl verifies all of its phase/admission gates before
  // it can call this receiver.
  event16.send_cut_sequence_start = [this, phase_one, phase_two](std::uint64_t sender) {
    const auto* movie = runtime_->movie_controller_reader_state();
    const auto* sequence = runtime_->first_cut_sequence_reader_state();
    if (!movie || !sequence || sender != movie->owner.value)
      throw std::runtime_error("MovieControl first-cut handoff sender relation changed");
    require_live_relations();
    player_->run_phase_one(phase_one);
    player_->run_phase_two(phase_two);
    if (!player_->initialization().phase_one_complete() ||
        !player_->initialization().phase_two_complete() || !player_->receiver().closed())
      throw std::runtime_error("MovieControl first-cut handoff lifecycle did not complete");
    delivered_ = true;
  };
  const auto result = movie_control.dispatch_event16(event16);
  if (result == MovieControlEvent16Result::activated && !delivered_)
    throw std::runtime_error("MovieControl activated without first-cut lifecycle delivery");
  return result;
}

}  // namespace off::graphics
