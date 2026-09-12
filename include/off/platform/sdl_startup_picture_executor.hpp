#pragma once

#include "off/graphics/startup_graphics_expanded_plan.hpp"
#include "off/platform/sdl_intro_renderer.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace off::platform {

// A live GPU texture registration, not an authored PRM record.  The caller
// obtains these only after the source-picture admission boundary has checked
// residency.  All three fields are checked: resource slots alone are not a
// stable texture identity.
struct StartupPictureTextureHandle final {
  std::size_t resource_index{};
  std::size_t catalog_image_index{};
  std::uint32_t texture_id{};
};

// Deliberately caller-owned.  This packet neither derives nor changes a pass,
// camera, projection, viewport, scissor, stage, texture factor, or blend/depth
// state.
struct StartupPictureRenderState final {
  std::array<float, 16> projection{};
  SDL_GPUViewport viewport{};
  SDL_Rect scissor{};
  graphics::PictureTrackedStage stage{};
  std::uint32_t packed_texture_factor{};
  SdlIntroDrawState draw_state{};
};

// Immutable CPU packet for one already-admitted ordered startup-picture span.
// It retains no asset, traversal, root, owner/view, or GPU texture pointer.
class SdlStartupPictureFramePacket final {
public:
  [[nodiscard]] static SdlStartupPictureFramePacket assemble(
      std::span<const graphics::StartupGraphicsExpandedSubmission> submissions,
      std::span<const StartupPictureTextureHandle> textures,
      const StartupPictureRenderState& state);
  [[nodiscard]] std::span<const SdlIntroDraw> draws() const noexcept {
    return draws_;
  }
  [[nodiscard]] std::size_t indexed_draw_count() const noexcept {
    return draws_.size();
  }

private:
  std::vector<graphics::ExpandedPictureBatch> batches_;
  std::vector<SdlIntroDraw> draws_;
};

// Prepared use of a caller command buffer.  It does not begin/end a pass,
// clear, submit, acquire a swapchain texture, or present.
class SdlStartupPictureFrame final {
public:
  SdlStartupPictureFrame(SdlStartupPictureFrame&&) noexcept = default;
  SdlStartupPictureFrame& operator=(SdlStartupPictureFrame&&) noexcept = default;
  SdlStartupPictureFrame(const SdlStartupPictureFrame&) = delete;
  SdlStartupPictureFrame& operator=(const SdlStartupPictureFrame&) = delete;
  void draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass) const;
  [[nodiscard]] std::size_t indexed_draw_count() const noexcept;

private:
  friend class SdlStartupPictureExecutor;
  explicit SdlStartupPictureFrame(std::unique_ptr<SdlIntroFrame> frame)
      : frame_(std::move(frame)) {}
  std::unique_ptr<SdlIntroFrame> frame_;
};

// Borrowed renderer adapter.  The renderer's image upload set must have been
// created from the same admitted texture identities; packet assembly verifies
// the resource-to-catalog mapping before this boundary is reached.
class SdlStartupPictureExecutor final {
public:
  explicit SdlStartupPictureExecutor(const SdlIntroRenderer& renderer)
      : renderer_(renderer) {}
  [[nodiscard]] SdlStartupPictureFrame prepare(
      SDL_GPUCommandBuffer* command,
      const SdlStartupPictureFramePacket& packet) const;

private:
  const SdlIntroRenderer& renderer_;
};

} // namespace off::platform
