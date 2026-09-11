#pragma once

#include "off/graphics/intro_preview_builder.hpp"
#include "off/platform/intro_picture_submission.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <span>
#include <utility>

namespace off::platform {

// An opt-in visual inspection path for one source-backed intro picture.  Its
// fit projection and baseline GPU state are project policy, not recovered
// scene state.  It must never be used as automatic intro playback.
class IntroPreviewDiagnosticSubmission final {
public:
  [[nodiscard]] static IntroPreviewDiagnosticSubmission build(
      const graphics::IntroPreviewSnapshot &snapshot, std::uint32_t width,
      std::uint32_t height, SDL_GPUTextureFormat color_format);

  [[nodiscard]] std::span<const SdlIntroDraw> draws() const noexcept {
    return submission_.draws();
  }

private:
  explicit IntroPreviewDiagnosticSubmission(IntroPictureSubmission submission)
      : submission_(std::move(submission)) {}

  IntroPictureSubmission submission_;
};

} // namespace off::platform
