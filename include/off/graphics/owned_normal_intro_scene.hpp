#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"
#include "off/graphics/selected_intro_camera_view_lease.hpp"

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
  // Available only after the source-backed first-cut player has been prepared.
  // Its preparation validates that the sequence camera reference, Window's
  // selected-camera reference, resource mapping, and live camera owner agree.
  [[nodiscard]] SelectedIntroCameraViewLease
  first_cut_camera_view_lease() const {
    const auto& runtime = session_->runtime();
    const auto* prepared = runtime.first_cut_player_prepared_state();
    if (!prepared || prepared->camera_owner.value == 0U ||
        prepared->camera_resource.value == 0U ||
        runtime.resource_handle(prepared->camera_owner) !=
            prepared->camera_resource)
      throw std::runtime_error(
          "first-cut camera view lease requires a live prepared camera");

    const auto& camera = runtime.camera_for_owner(prepared->camera_owner);
    auto owner = shared_from_this();
    std::shared_ptr<const FreshIntroCamera> camera_alias(
        owner, std::addressof(camera));
    return SelectedIntroCameraViewLease(
        std::move(owner), std::move(camera_alias), prepared->camera_owner.value);
  }

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
