#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace off::graphics {

// Owns the concrete application and component-sequence dependencies borrowed
// by this one session. This is a C++ lifetime owner only: it is not an engine
// scene-manager lease, an activation receipt, or an admitted camera/view.
// Callers must ensure that callbacks stored in either dependency retain their
// own external captures for at least this bundle's lifetime.
class OwnedNormalIntroScene final : public std::enable_shared_from_this<OwnedNormalIntroScene> {
public:
  [[nodiscard]] static std::shared_ptr<OwnedNormalIntroScene> create(
      IntroPreparedResources&& resources,
      std::shared_ptr<runtime::ApplicationServices> application,
      std::shared_ptr<runtime::SceneComponentSequence> sequence,
      std::string scene_filename,
      IntroSoundLoadPolicy sound_policy) {
    if (!application || !sequence)
      throw std::invalid_argument("owned intro scene requires application and component sequence owners");
    return std::shared_ptr<OwnedNormalIntroScene>(new OwnedNormalIntroScene(
        std::move(resources),std::move(application),std::move(sequence),
        std::move(scene_filename),sound_policy));
  }
  OwnedNormalIntroScene(const OwnedNormalIntroScene&) = delete;
  OwnedNormalIntroScene& operator=(const OwnedNormalIntroScene&) = delete;
  [[nodiscard]] NormalIntroSceneSession& session() noexcept { return *session_; }
  [[nodiscard]] const NormalIntroSceneSession& session() const noexcept { return *session_; }
  [[nodiscard]] std::shared_ptr<const void> lifetime_owner() const { return shared_from_this(); }

private:
  OwnedNormalIntroScene(IntroPreparedResources&& resources,
      std::shared_ptr<runtime::ApplicationServices> application,
      std::shared_ptr<runtime::SceneComponentSequence> sequence,
      std::string scene_filename,IntroSoundLoadPolicy sound_policy)
      : application_(std::move(application)), sequence_(std::move(sequence)),
        session_(make_normal_intro_scene_session(std::make_unique<IntroRuntime>(
            std::move(resources),*application_,*sequence_,std::move(scene_filename),sound_policy))) {}
  // Reverse destruction order: session before its borrowed sequence/application.
  std::shared_ptr<runtime::ApplicationServices> application_;
  std::shared_ptr<runtime::SceneComponentSequence> sequence_;
  std::unique_ptr<NormalIntroSceneSession> session_;
};

} // namespace off::graphics
