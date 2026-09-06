#pragma once
#include "off/graphics/intro_controller_initialization.hpp"
#include <SDL3/SDL.h>
#include <memory>

namespace off::platform {
// Native healthy-device phase-two presentation adapter. Window must already be
// claimed for device, and both must outlive this adapter and its bound callbacks.
// Use on the window-creation thread with exclusive command/window access.
// Explicit depth format is backend policy, not inferred original device state.
// Supports stable positive dimensions and a full-window viewport only; resizing,
// minimized/no swapchain, device loss and configuration reset require host action.
// Caller must exclude pending configuration/reset requests and provide applicable
// profiling: dimension/format checks do not detect same-format present-mode changes
// or establish the original engine's pending-reset flag is clear.
class SdlIntroPresentation final {
public:
  SdlIntroPresentation(SDL_GPUDevice* device,SDL_Window* window,
                       std::uint64_t renderer_identity,SDL_GPUTextureFormat depth_format);
  ~SdlIntroPresentation();
  SdlIntroPresentation(const SdlIntroPresentation&)=delete;
  SdlIntroPresentation& operator=(const SdlIntroPresentation&)=delete;
  // Replaces only renderer height/width/viewport/stencil/clear/present callbacks.
  // first_renderer remains the caller's live engine lookup. Input/global/audio/
  // clock services and phase-two admission are not supplied by this adapter.
  // Binding is transactional. Query/viewport preflight failures leave GPU state
  // unchanged and do not poison; clear/present failures cancel/finish pending
  // work and poison the adapter, including a second clear before presentation.
  void bind_renderer_services(graphics::IntroControllerPhaseTwoServices& services);
  [[nodiscard]] std::uint64_t completed_presentations() const noexcept;
  [[nodiscard]] bool failed() const noexcept;
  // Pending clears target private backbuffers and are cancellable. Presentation
  // acquires the swapchain last; SDL forbids cancellation after that acquisition.
  // A post-acquisition failure therefore performs terminal submission (possibly
  // presenting) and reports failure, never successful phase-two completion.
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace off::platform
