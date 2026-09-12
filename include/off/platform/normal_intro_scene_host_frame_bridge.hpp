#pragma once

#include "off/graphics/normal_intro_scene_host.hpp"
#include "off/platform/sdl_first_cut_picture_frame_bridge.hpp"

#include <memory>
#include <stdexcept>

namespace off::platform {

// Production hand-off for a first-cut frame that has already passed the
// normal-scene host's lifecycle, event, camera-route, and view-admission
// boundaries. It adds no renderer state, source selection, projection, audio,
// or presentation behavior. The host and its frame must outlive this bridge.
class NormalIntroSceneHostFrameBridge final {
 public:
  [[nodiscard]] static NormalIntroSceneHostFrameBridge bind(
      const graphics::NormalIntroSceneHost& host) {
    if (host.stage() != graphics::NormalIntroSceneHostStage::frame_assembled)
      throw std::runtime_error(
          "normal intro scene host has not assembled an admitted first-cut frame");
    return NormalIntroSceneHostFrameBridge(host,
        SdlFirstCutPictureFrameBridge::bind(host.first_cut_frame()));
  }

  NormalIntroSceneHostFrameBridge(const NormalIntroSceneHostFrameBridge&) = delete;
  NormalIntroSceneHostFrameBridge& operator=(const NormalIntroSceneHostFrameBridge&) = delete;
  NormalIntroSceneHostFrameBridge(NormalIntroSceneHostFrameBridge&&) noexcept = default;
  NormalIntroSceneHostFrameBridge& operator=(NormalIntroSceneHostFrameBridge&&) noexcept = default;

  [[nodiscard]] std::span<const SdlIntroDraw> draws() const {
    validate_host();
    return frame_.draws();
  }

  // Uses the caller-owned command buffer and renderer. This never begins a
  // pass, submits a command buffer, or presents a frame.
  [[nodiscard]] std::unique_ptr<SdlIntroFrame> prepare(
      SdlIntroRenderer& renderer, SDL_GPUCommandBuffer* command) const {
    validate_host();
    return frame_.prepare(renderer, command);
  }

 private:
  NormalIntroSceneHostFrameBridge(const graphics::NormalIntroSceneHost& host,
                                  SdlFirstCutPictureFrameBridge frame)
      : host_(std::addressof(host)), frame_(std::move(frame)) {}

  void validate_host() const {
    if (!host_ || host_->stage() != graphics::NormalIntroSceneHostStage::frame_assembled)
      throw std::runtime_error("normal intro scene host frame binding is stale");
  }

  const graphics::NormalIntroSceneHost* host_{};
  SdlFirstCutPictureFrameBridge frame_;
};

}  // namespace off::platform
