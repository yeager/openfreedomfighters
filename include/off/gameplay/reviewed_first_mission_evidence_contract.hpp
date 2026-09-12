#pragma once

#include <filesystem>
#include <optional>

namespace off::gameplay {

// These are source-free observation categories. They deliberately do not name
// a scene, a retail resource, an input binding, or any numeric game behavior.
enum class FirstMissionProbe {
  movement, look, movement_look, fire, aim, interact, squad, pause, menu,
  obstacle, interaction_candidate,
};
enum class FirstMissionBoundary {
  handoff, control, camera, player, collision, interaction, mission, hud,
};
enum class FirstMissionObservedState {
  cinematic_visible, loading_visible, gameplay_viewport_visible,
  no_response, movement_only, camera_only, movement_and_camera, blocked_by_overlay,
  fixed, follows_translation, rotates_with_look, independently_rotatable,
  absent, spawned, controllable, disabled,
  no_contact, blocked, sliding, stepped, falling,
  unavailable, prompt_only, entered_range, accepted, rejected,
  unchanged, objective_advanced, failed, completed, loading, stable, changed, hidden,
  unknown,
};

struct FirstMissionEvidenceFacts final {
  FirstMissionProbe probe{};
  FirstMissionBoundary visible_boundary{};
  FirstMissionObservedState visible_state{};
  FirstMissionObservedState terminal_state{};
};

// A local receipt for the aggregate emitted by
// first_mission_observation_repeat_bundle.py.  It is intentionally inert: no
// runtime subsystem consumes it and it contains no data identity or metadata.
class ReviewedFirstMissionEvidenceContract final {
 public:
  [[nodiscard]] static std::optional<ReviewedFirstMissionEvidenceContract>
  load_local(const std::filesystem::path& local_directory);

  [[nodiscard]] bool admitted() const noexcept { return true; }
  [[nodiscard]] const FirstMissionEvidenceFacts& facts() const noexcept { return facts_; }

 private:
  explicit ReviewedFirstMissionEvidenceContract(FirstMissionEvidenceFacts facts)
      : facts_(facts) {}
  FirstMissionEvidenceFacts facts_;
};

}  // namespace off::gameplay
