#pragma once

#include "off/cutscene/first_cut_player_initialization.hpp"
#include "off/cutscene/first_cut_clocked_command_runner.hpp"
#include "off/graphics/intro_outer_loader_tail_readiness.hpp"
#include "off/graphics/movie_control_first_cut_runtime_handoff.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace off::graphics {
class IntroRuntime;
struct IntroOuterLoaderTailServices;

// These stages cover loader work only. They are not scene activation, cut
// admission, rendering, or playback evidence.
enum class NormalIntroSceneSessionStage : std::uint8_t {
  postconstructed,
  reader_bracket_complete,
  outer_loader_tail_complete,
  failed,
};

// Records ordering-only hooks reached by the reviewed reader bracket.  These
// observations deliberately do not turn opaque loader callbacks into inferred
// runtime behavior; they keep their source inputs available for later recovery.
struct NormalIntroReaderBracketObservation {
  std::vector<std::uint64_t> external_loader_values;
  std::vector<IntroSourceScriptWork> source_scripts;
  std::size_t pre_reader_calls{};
  std::size_t prepared_reader_calls{};
  std::size_t end_reader_calls{};
};

class NormalIntroSceneSession final {
public:
  explicit NormalIntroSceneSession(std::unique_ptr<IntroRuntime> runtime);
  ~NormalIntroSceneSession();
  NormalIntroSceneSession(const NormalIntroSceneSession &) = delete;
  NormalIntroSceneSession &operator=(const NormalIntroSceneSession &) = delete;
  void complete_postconstruction_reader_bracket(std::uint64_t saved);
  // Binds the reviewed, still-cold first-cut command session to the same
  // scene-owned lifetime as its source runtime. This does not execute either
  // lifecycle phase, schedule a cut, start audio, or admit rendering.
  void prepare_supported_first_cut_player();
  // Builds the source-backed command router and its scene-clock runner under
  // this session's lifetime. The caller still owns lifecycle admission, cut
  // start/stop, clock sampling, and concrete target/component behavior.
  void prepare_first_cut_command_runner(
      float derived_end,
      runtime::IntroLiveTargetRegistry::DispatchServices dispatch);
  void complete_outer_loader_tail(const IntroOuterLoaderTailServices &services);
  [[nodiscard]] NormalIntroSceneSessionStage stage() const noexcept {
    return stage_;
  }
  [[nodiscard]] IntroRuntime &runtime() noexcept { return *runtime_; }
  [[nodiscard]] const IntroRuntime &runtime() const noexcept {
    return *runtime_;
  }
  [[nodiscard]] const cutscene::FirstCutPlayerSession *first_cut_player() const noexcept {
    return first_cut_player_ ? std::addressof(*first_cut_player_) : nullptr;
  }
  [[nodiscard]] cutscene::FirstCutPlayerSession *first_cut_player() noexcept {
    return first_cut_player_ ? std::addressof(*first_cut_player_) : nullptr;
  }
  // Produces the narrow MovieControl receiver only after the reader-owned
  // runtime and its cold first-cut session coexist. It does not dispatch it.
  [[nodiscard]] MovieControlFirstCutRuntimeHandoff make_first_cut_handoff();
  [[nodiscard]] cutscene::FirstCutClockedCommandRunner *first_cut_command_runner() noexcept {
    return first_cut_command_runner_.get();
  }
  [[nodiscard]] const cutscene::FirstCutClockedCommandRunner *first_cut_command_runner() const noexcept {
    return first_cut_command_runner_.get();
  }
  [[nodiscard]] IntroOuterLoaderTailReadiness outer_loader_tail_readiness() const;
  [[nodiscard]] const NormalIntroReaderBracketObservation &
  reader_bracket_observation() const noexcept {
    return reader_bracket_observation_;
  }

private:
  std::unique_ptr<IntroRuntime> runtime_;
  std::optional<cutscene::FirstCutPlayerSession> first_cut_player_;
  std::unique_ptr<cutscene::FirstCutRuntimeCommandRouter> first_cut_command_router_;
  std::unique_ptr<cutscene::FirstCutCommandSession> first_cut_command_session_;
  std::unique_ptr<cutscene::FirstCutClockedCommandRunner> first_cut_command_runner_;
  NormalIntroReaderBracketObservation reader_bracket_observation_;
  NormalIntroSceneSessionStage stage_{
      NormalIntroSceneSessionStage::postconstructed};
};

[[nodiscard]] std::unique_ptr<NormalIntroSceneSession>
make_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime);
// Use this ownership form when retaining lifecycle adapters beyond the caller's
// immediate loader scope. It is still inert until an explicit host activation.
[[nodiscard]] std::shared_ptr<NormalIntroSceneSession>
make_shared_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime);
} // namespace off::graphics
