#pragma once

#include "off/graphics/picture_expansion.hpp"
#include "off/platform/sdl_intro_renderer.hpp"

#include <vector>

namespace off::platform {

// Explicitly admitted picture data for one GPU submission. The caller owns
// scene selection, visibility, camera/view admission and all render state.
struct IntroPictureSubmissionInput final {
  std::span<const data::BoundPictureDrawGroup> groups;
  graphics::PictureCacheTransform transform;
  std::array<float, 16> projection;
  SDL_GPUViewport viewport;
  SDL_Rect scissor;
  graphics::PictureTrackedStage stage;
  std::uint32_t packed_texture_factor{};
  SdlIntroDrawState state;
};

// Keeps expanded batches alive while SdlIntroRenderer consumes its draw spans.
// It makes no admission, ordering, visibility, camera or render-state choice.
class IntroPictureSubmission final {
public:
  [[nodiscard]] static IntroPictureSubmission assemble(
      const IntroPictureSubmissionInput &input);
  [[nodiscard]] std::span<const SdlIntroDraw> draws() const noexcept {
    return draws_;
  }

private:
  std::vector<std::vector<graphics::ExpandedPictureBatch>> batches_;
  std::vector<SdlIntroDraw> draws_;
};

} // namespace off::platform
