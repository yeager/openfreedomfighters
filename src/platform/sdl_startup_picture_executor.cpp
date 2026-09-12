#include "off/platform/sdl_startup_picture_executor.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace off::platform {

bool SdlStartupPictureTextureRegistry::has_valid_asset_identities(
    std::span<const graphics::StartupGraphicsImage> images) noexcept {
  if (images.size() != graphics::startup_graphics_image_count)
    return false;
  for (std::size_t index = 0; index < images.size(); ++index) {
    if (images[index].texture_id == 0U)
      return false;
    for (std::size_t prior = 0; prior < index; ++prior)
      if (images[prior].catalog_image_index == images[index].catalog_image_index ||
          images[prior].texture_id == images[index].texture_id)
        return false;
  }
  return true;
}

SdlStartupPictureTextureRegistry::SdlStartupPictureTextureRegistry(
    SDL_GPUDevice* device, const graphics::StartupGraphicsAsset& asset) {
  const auto& source = asset.images();
  if (!has_valid_asset_identities(source))
    throw std::runtime_error("startup texture registry has invalid asset identities");
  std::vector<graphics::IntroPreparedImage> images;
  images.reserve(source.size());
  identities_.reserve(source.size());
  for (const auto& image : source) {
    identities_.emplace_back(image.catalog_image_index, image.texture_id);
    images.push_back({image.catalog_image_index, image.texture_id, image.mip_zero});
  }
  renderer_ = std::make_unique<SdlIntroRenderer>(device, images);
}

SdlStartupPictureTextureRegistry::~SdlStartupPictureTextureRegistry() = default;

std::size_t SdlStartupPictureTextureRegistry::image_count() const noexcept {
  return identities_.size();
}

void SdlStartupPictureTextureRegistry::verify(
    std::span<const StartupPictureTextureHandle> handles) const {
  for (const auto& handle : handles) {
    const auto found = std::find(identities_.begin(), identities_.end(),
                                 std::pair{handle.catalog_image_index, handle.texture_id});
    if (found == identities_.end())
      throw std::runtime_error("startup picture packet identity is not owned by registry");
  }
}

SdlStartupPictureFramePacket SdlStartupPictureFramePacket::assemble(
    std::span<const graphics::StartupGraphicsExpandedSubmission> submissions,
    std::span<const graphics::StartupGraphicsPreparedResource> resources,
    std::span<const StartupPictureTextureHandle> textures,
    const StartupPictureRenderState& state) {
  if (submissions.empty())
    throw std::runtime_error("startup picture packet requires submissions");
  std::unordered_map<std::size_t, StartupPictureTextureHandle> handles;
  handles.reserve(textures.size());
  for (const auto& handle : textures) {
    if (handle.texture_id == 0U ||
        !handles.emplace(handle.resource_index, handle).second)
      throw std::runtime_error("startup picture packet has duplicate or invalid texture identity");
    for (const auto& prior : textures) {
      if (&prior == &handle) break;
      if (prior.catalog_image_index == handle.catalog_image_index ||
          prior.texture_id == handle.texture_id)
        throw std::runtime_error("startup picture packet has duplicate texture identity");
    }
  }

  SdlStartupPictureFramePacket result;
  result.batches_.reserve(submissions.size());
  result.draws_.reserve(submissions.size());
  std::unordered_set<std::size_t> used;
  for (std::size_t ordinal = 0; ordinal < submissions.size(); ++ordinal) {
    const auto& submission = submissions[ordinal];
    if (submission.emission_ordinal != ordinal)
      throw std::runtime_error("startup picture packet submission order is not admitted order");
    const auto found = handles.find(submission.resource_index);
    if (found == handles.end())
      throw std::runtime_error("startup picture packet submission resource is unknown");
    const auto resource = std::find_if(resources.begin(), resources.end(),
        [&](const auto& candidate) { return candidate.resource_index == submission.resource_index; });
    if (resource == resources.end() ||
        resource->catalog_image_index != found->second.catalog_image_index ||
        resource->texture_id != found->second.texture_id)
      throw std::runtime_error("startup picture packet resource texture identity mismatches admission");
    if (std::find_if(std::next(resource), resources.end(),
        [&](const auto& candidate) { return candidate.resource_index == submission.resource_index; }) != resources.end())
      throw std::runtime_error("startup picture packet has ambiguous prepared resource identity");
    used.insert(submission.resource_index);
    result.texture_handles_.push_back(found->second);
    graphics::ExpandedPictureBatch batch;
    batch.first_descriptor = submission.descriptor_index;
    batch.vertices.assign(submission.vertices.begin(), submission.vertices.end());
    batch.indices.assign(submission.indices.begin(), submission.indices.end());
    result.batches_.push_back(std::move(batch));
    SdlIntroDraw draw{};
    draw.batches = std::span<const graphics::ExpandedPictureBatch>(
        &result.batches_.back(), 1U);
    draw.catalog_image_index = found->second.catalog_image_index;
    draw.projection = state.projection;
    draw.viewport = state.viewport;
    draw.scissor = state.scissor;
    draw.stage = state.stage;
    draw.packed_texture_factor = state.packed_texture_factor;
    draw.state = state.draw_state;
    result.draws_.push_back(draw);
  }
  if (used.size() != handles.size())
    throw std::runtime_error("startup picture packet texture identity is not used by admitted submissions");
  return result;
}

void SdlStartupPictureFrame::draw(SDL_GPUCommandBuffer* command,
                                  SDL_GPURenderPass* pass) const {
  if (!frame_) throw std::runtime_error("startup picture frame is empty");
  frame_->draw(command, pass);
}

std::size_t SdlStartupPictureFrame::indexed_draw_count() const noexcept {
  return frame_ ? frame_->indexed_draw_count() : 0U;
}

SdlStartupPictureFrame SdlStartupPictureExecutor::prepare(
    SDL_GPUCommandBuffer* command,
    const SdlStartupPictureFramePacket& packet) const {
  if (packet.draws().empty())
    throw std::runtime_error("startup picture executor requires an admitted packet");
  if (registry_ != nullptr)
    registry_->verify(packet.texture_handles_);
  return SdlStartupPictureFrame(renderer_.prepare(command, packet.draws()));
}

} // namespace off::platform
