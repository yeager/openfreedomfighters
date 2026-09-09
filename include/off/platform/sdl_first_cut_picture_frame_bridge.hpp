#pragma once

#include "off/platform/first_cut_picture_frame.hpp"
#include "off/platform/sdl_intro_renderer.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace off::platform {

// A deliberately narrow SDL boundary for a FirstCutPictureFrame the caller
// has already admitted and assembled.  It owns no scene selection, clock,
// lifecycle state, GPU pass, submission or presentation.  The supplied frame
// must outlive this bridge and the SdlIntroFrame returned by prepare(), and
// must not be reassembled while either is used.
class SdlFirstCutPictureFrameBridge final {
public:
  [[nodiscard]] static SdlFirstCutPictureFrameBridge bind(
      const FirstCutPictureFrame& frame) {
    if (!frame.ready_for_render())
      throw std::runtime_error("first-cut picture frame is not admitted for rendering");
    SdlFirstCutPictureFrameBridge result(frame);
    for (const auto& submission : frame.submissions()) {
      const auto draws = submission.draws();
      result.draws_.insert(result.draws_.end(), draws.begin(), draws.end());
    }
    if (result.draws_.empty())
      throw std::runtime_error("admitted first-cut picture frame has no draws");
    return result;
  }

  SdlFirstCutPictureFrameBridge(const SdlFirstCutPictureFrameBridge&) = delete;
  SdlFirstCutPictureFrameBridge& operator=(const SdlFirstCutPictureFrameBridge&) = delete;
  SdlFirstCutPictureFrameBridge(SdlFirstCutPictureFrameBridge&&) noexcept = default;
  SdlFirstCutPictureFrameBridge& operator=(SdlFirstCutPictureFrameBridge&&) noexcept = default;

  [[nodiscard]] std::span<const SdlIntroDraw> draws() const {
    validate_binding();
    return draws_;
  }

  // Uses the caller's command buffer exactly as SdlIntroRenderer::prepare().
  // This bridge never begins a render pass or submits/presents that buffer.
  [[nodiscard]] std::unique_ptr<SdlIntroFrame> prepare(
      SdlIntroRenderer& renderer, SDL_GPUCommandBuffer* command) const {
    validate_binding();
    return renderer.prepare(command, draws_);
  }

private:
  explicit SdlFirstCutPictureFrameBridge(const FirstCutPictureFrame& frame)
      : frame_(std::addressof(frame)), generation_(frame.assembly_generation()) {}

  void validate_binding() const {
    if (!frame_ || !frame_->ready_for_render() ||
        frame_->assembly_generation() != generation_)
      throw std::runtime_error("first-cut picture frame binding is stale");
  }

  const FirstCutPictureFrame* frame_{};
  std::uint64_t generation_{};
  std::vector<SdlIntroDraw> draws_;
};

} // namespace off::platform
