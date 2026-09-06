#pragma once

#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/picture_expansion.hpp"
#include "off/platform/picture_stage_shader.hpp"
#include <memory>

namespace off::platform {

struct SdlIntroDrawState {
  // Linear RGBA8/BGRA8, single sample. No comparison sampler or extension props.
  // Inactive SDL enum slots may be INVALID; active operations must be explicit.
  SDL_GPUSamplerCreateInfo sampler;
  SDL_GPURasterizerState rasterizer;
  SDL_GPUColorTargetBlendState blend;
  SDL_GPUDepthStencilState depth_stencil;
  SDL_GPUTextureFormat color_format;
  SDL_GPUTextureFormat depth_stencil_format; // INVALID means no attachment.
  SDL_FColor blend_constants;
  Uint8 stencil_reference;
  // These unsupported fragment features must be explicitly resolved false/zero.
  bool fog_enabled;
  bool alpha_test_enabled;
  unsigned active_later_stages;
};

struct SdlIntroDraw {
  std::span<const graphics::ExpandedPictureBatch> batches;
  std::size_t catalog_image_index;
  // Column-major clip projection. Expanded XYZ already includes object/view
  // transforms; the vertex shader receives an identity model matrix.
  std::array<float,16> projection;
  SDL_GPUViewport viewport;
  SDL_Rect scissor;
  graphics::PictureTrackedStage stage;
  std::uint32_t packed_texture_factor;
  SdlIntroDrawState state;
};

class SdlIntroRenderer;
class SdlIntroFrame final {
public:
  ~SdlIntroFrame();
  SdlIntroFrame(const SdlIntroFrame&) = delete;
  SdlIntroFrame& operator=(const SdlIntroFrame&) = delete;
  // Uses the SAME command buffer passed to prepare(), after its copy pass.
  // Caller supplies a compatible single-sample mip-zero render pass, attachment
  // formats/dimensions covering the requested viewport/scissor, and load/store.
  // Binds every draw dependency; does not clear, end the pass, or submit.
  void draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass) const;
  [[nodiscard]] std::size_t indexed_draw_count() const noexcept;
private:
  friend class SdlIntroRenderer;
  struct Impl;
  explicit SdlIntroFrame(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

// Device is borrowed and must outlive renderer AND all prepared frames.
// Images are uploaded exactly once per instance; pixel sources need only live
// through construction. No fallback texture, original admission or scene timing.
class SdlIntroRenderer final {
public:
  SdlIntroRenderer(SDL_GPUDevice* device,
                   std::span<const graphics::IntroPreparedImage> images);
  ~SdlIntroRenderer();
  SdlIntroRenderer(const SdlIntroRenderer&) = delete;
  SdlIntroRenderer& operator=(const SdlIntroRenderer&) = delete;
  [[nodiscard]] std::size_t image_count() const noexcept;
  // No active pass may exist. Validates CPU inputs before encoding commands;
  // uploads indexed expanded geometry in one copy pass. Input spans are copied.
  // Native safety limits: <=65536 vertices per batch and <=64 MiB each for
  // frame vertex/index data. All numeric GPU state must be finite.
  // Caller must submit+finish or cancel the command BEFORE destroying the frame.
  // Waiting for GPU idle cannot make an unsubmitted command safe. On exception,
  // cancel the caller-owned command; no implicit submission/retry is performed.
  [[nodiscard]] std::unique_ptr<SdlIntroFrame> prepare(
      SDL_GPUCommandBuffer* command, std::span<const SdlIntroDraw> draws) const;
private:
  friend struct SdlIntroFrame::Impl;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};
} // namespace off::platform
