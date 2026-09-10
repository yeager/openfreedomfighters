#pragma once

#include <cstdint>
#include <memory>

namespace off::graphics {
class IntroRuntime;

class NormalIntroSceneReaderBoundary {
public:
  virtual ~NormalIntroSceneReaderBoundary() = default;
  virtual void
  complete_postconstruction_reader_bracket(std::uint64_t saved) = 0;
  [[nodiscard]] virtual IntroRuntime *runtime() noexcept = 0;
};

// This stage records only the reviewed reader bracket. It is not scene
// activation, cut admission, rendering, or playback evidence.
enum class NormalIntroSceneSessionStage : std::uint8_t {
  postconstructed,
  reader_bracket_complete,
  failed,
};

class NormalIntroSceneSession final {
public:
  explicit NormalIntroSceneSession(
      std::unique_ptr<NormalIntroSceneReaderBoundary> boundary);
  NormalIntroSceneSession(const NormalIntroSceneSession &) = delete;
  NormalIntroSceneSession &operator=(const NormalIntroSceneSession &) = delete;
  void complete_postconstruction_reader_bracket(std::uint64_t saved);
  [[nodiscard]] NormalIntroSceneSessionStage stage() const noexcept {
    return stage_;
  }
  [[nodiscard]] IntroRuntime &runtime() const;

private:
  std::unique_ptr<NormalIntroSceneReaderBoundary> boundary_;
  NormalIntroSceneSessionStage stage_{
      NormalIntroSceneSessionStage::postconstructed};
};

[[nodiscard]] std::unique_ptr<NormalIntroSceneSession>
make_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime);
} // namespace off::graphics
