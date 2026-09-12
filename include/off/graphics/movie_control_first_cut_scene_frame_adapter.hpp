#pragma once

#include "off/cutscene/first_cut_player_initialization.hpp"
#include "off/graphics/movie_control_event16_admission_snapshot.hpp"
#include "off/graphics/movie_control_first_cut_runtime_handoff.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace off::graphics {

// Live values supplied by the ordinary-component owner for one frame.  The
// adapter captures these through MovieControlEvent16ManagerSnapshot before it
// touches the retained handoff; it never traverses or dispatches the manager.
struct MovieControlFirstCutSceneFrameInputs final {
  runtime::OrdinaryComponentManager& manager;
  const runtime::ComponentRecord& component;
  std::uint64_t component_handle{};
  std::int32_t scene_integer{};
  bool paused{};
  std::optional<std::uint64_t> component_filter;
};

// All effectful operations remain direct caller-owned services.  No scheduler,
// clock source, camera, audio, renderer, view, or presentation service can be
// supplied by this narrow frame adapter.
struct MovieControlFirstCutSceneFrameServices final {
  MovieControlEvent16ManagerSnapshot::DirectServices direct;
  cutscene::FirstCutPlayerPhaseOneServices phase_one;
  cutscene::FirstCutPlayerSessionPhaseTwoServices phase_two;
};

// Retains the already checked typed MovieControl-to-first-cut receiver and
// performs one ordinary event-16 frame attempt.  It is deliberately
// disconnected from normal startup.  A frame that is skipped or waits leaves
// the player cold; only MovieControlFirstUpdate's activated result can deliver
// the retained handoff.  Once delivery succeeds, this adapter is exhausted.
class MovieControlFirstCutSceneFrameAdapter final {
 public:
  [[nodiscard]] static MovieControlFirstCutSceneFrameAdapter bind(
      MovieControlFirstUpdate& movie_control,
      MovieControlFirstCutRuntimeHandoff handoff);

  MovieControlFirstCutSceneFrameAdapter(const MovieControlFirstCutSceneFrameAdapter&) = delete;
  MovieControlFirstCutSceneFrameAdapter& operator=(const MovieControlFirstCutSceneFrameAdapter&) = delete;
  MovieControlFirstCutSceneFrameAdapter(MovieControlFirstCutSceneFrameAdapter&&) = default;
  MovieControlFirstCutSceneFrameAdapter& operator=(MovieControlFirstCutSceneFrameAdapter&&) = default;

  [[nodiscard]] MovieControlEvent16Result run_frame(
      const MovieControlFirstCutSceneFrameInputs& inputs,
      MovieControlFirstCutSceneFrameServices services);
  [[nodiscard]] bool delivered() const noexcept { return delivered_; }

 private:
  MovieControlFirstCutSceneFrameAdapter(MovieControlFirstUpdate& movie_control,
                                        MovieControlFirstCutRuntimeHandoff handoff)
      : movie_control_(&movie_control), handoff_(std::move(handoff)) {}

  MovieControlFirstUpdate* movie_control_{};
  MovieControlFirstCutRuntimeHandoff handoff_;
  bool delivered_{};
};

}  // namespace off::graphics
