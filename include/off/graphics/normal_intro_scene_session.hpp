#pragma once

#include "off/cutscene/first_cut_player_initialization.hpp"

#include <cstdint>
#include <memory>
#include <optional>

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

private:
  std::unique_ptr<IntroRuntime> runtime_;
  std::optional<cutscene::FirstCutPlayerSession> first_cut_player_;
  NormalIntroSceneSessionStage stage_{
      NormalIntroSceneSessionStage::postconstructed};
};

[[nodiscard]] std::unique_ptr<NormalIntroSceneSession>
make_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime);
} // namespace off::graphics
