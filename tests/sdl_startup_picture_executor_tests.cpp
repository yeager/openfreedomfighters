#include "off/platform/sdl_startup_picture_executor.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class F> void rejects(F&& action) {
  bool rejected = false;
  try { action(); } catch (const std::runtime_error&) { rejected = true; }
  check(rejected, "expected startup packet rejection");
}

off::graphics::StartupGraphicsExpandedSubmission submission(
    std::size_t ordinal, std::size_t resource, std::size_t descriptor) {
  off::graphics::StartupGraphicsExpandedSubmission result{};
  result.emission_ordinal = ordinal;
  result.resource_index = resource;
  result.descriptor_index = descriptor;
  result.vertices = std::array<off::graphics::ExpandedPictureVertex, 4>{
      off::graphics::ExpandedPictureVertex{{0, 0, 0}, {0, 0}, 0xffffffffU},
      off::graphics::ExpandedPictureVertex{{1, 0, 0}, {1, 0}, 0xffffffffU},
      off::graphics::ExpandedPictureVertex{{1, 1, 0}, {1, 1}, 0xffffffffU},
      off::graphics::ExpandedPictureVertex{{0, 1, 0}, {0, 1}, 0xffffffffU}};
  result.indices = {0, 1, 2, 0, 2, 3};
  return result;
}

// This fake is intentionally CPU-only: it proves a caller can consume the
// immutable packet without smuggling a root, camera, pass, or timing decision
// through the executor API.
struct FakeExecutor final {
  std::vector<std::size_t> catalog_indexes;
  void prepare(const off::platform::SdlStartupPictureFramePacket& packet) {
    for (const auto& draw : packet.draws()) catalog_indexes.push_back(draw.catalog_image_index);
  }
};
} // namespace

int main() {
  using namespace off;
  std::array<graphics::StartupGraphicsImage, graphics::startup_graphics_image_count> asset_images{};
  for (std::size_t index = 0; index < asset_images.size(); ++index) {
    asset_images[index].catalog_image_index = index + 10;
    asset_images[index].texture_id = static_cast<std::uint32_t>(index + 100);
  }
  check(platform::SdlStartupPictureTextureRegistry::has_valid_asset_identities(asset_images),
        "registry accepts six unique full startup identities");
  asset_images[5].texture_id = asset_images[0].texture_id;
  check(!platform::SdlStartupPictureTextureRegistry::has_valid_asset_identities(asset_images),
        "registry rejects duplicate startup texture identities");
  const std::array submissions{submission(0, 4, 17), submission(1, 2, 19)};
  const std::array textures{
      platform::StartupPictureTextureHandle{2, 91, 701},
      platform::StartupPictureTextureHandle{4, 33, 402}};
  const std::array resources{
      graphics::StartupGraphicsPreparedResource{2, 91, 701, 1, 1},
      graphics::StartupGraphicsPreparedResource{4, 33, 402, 1, 1}};
  platform::StartupPictureRenderState state{};
  state.projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  state.viewport = {5, 7, 80, 60, 0, 1};
  state.scissor = {5, 7, 80, 60};
  const auto packet = platform::SdlStartupPictureFramePacket::assemble(submissions, resources, textures, state);
  FakeExecutor fake;
  fake.prepare(packet);
  check(packet.indexed_draw_count() == 2 && fake.catalog_indexes == std::vector<std::size_t>{33, 91},
        "packet preserves admitted submission order and checked texture mapping");
  check(packet.draws()[0].batches[0].first_descriptor == 17 &&
            packet.draws()[0].viewport.x == 5 && packet.draws()[0].scissor.w == 80,
        "packet retains caller render state without deriving it");

  auto unordered = submissions;
  unordered[1].emission_ordinal = 9;
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(unordered, resources, textures, state)); });
  const std::array missing{textures[0]};
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(submissions, resources, missing, state)); });
  const std::array duplicate_resource{
      textures[0], platform::StartupPictureTextureHandle{2, 92, 702}};
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(submissions, resources, duplicate_resource, state)); });
  // Resource slots are the packet keys. Two separately admitted resources can
  // intentionally alias the same catalog image/GPU texture pair.
  const std::array aliased_submissions{submission(0, 2, 17), submission(1, 4, 19)};
  const std::array aliased_textures{
      platform::StartupPictureTextureHandle{2, 91, 701},
      platform::StartupPictureTextureHandle{4, 91, 701}};
  const std::array aliased_resources{
      graphics::StartupGraphicsPreparedResource{2, 91, 701, 1, 1},
      graphics::StartupGraphicsPreparedResource{4, 91, 701, 1, 1}};
  const auto aliased_packet = platform::SdlStartupPictureFramePacket::assemble(
      aliased_submissions, aliased_resources, aliased_textures, state);
  check(aliased_packet.indexed_draw_count() == 2 &&
            aliased_packet.draws()[0].catalog_image_index == 91 &&
            aliased_packet.draws()[1].catalog_image_index == 91,
        "packet permits distinct resource handles sharing one exact texture identity");
  const std::array zero_submissions{submission(0, 0, 17)};
  const std::array zero_textures{platform::StartupPictureTextureHandle{0, 0, 0}};
  const std::array zero_resources{
      graphics::StartupGraphicsPreparedResource{0, 0, 0, 1, 1}};
  const auto zero_packet = platform::SdlStartupPictureFramePacket::assemble(
      zero_submissions, zero_resources, zero_textures, state);
  check(zero_packet.indexed_draw_count() == 1,
        "zero-valued source texture identity is valid");
  const std::array duplicate_prepared_resources{
      graphics::StartupGraphicsPreparedResource{2, 91, 701, 1, 1},
      graphics::StartupGraphicsPreparedResource{2, 33, 402, 1, 1}};
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(
      submissions, duplicate_prepared_resources, textures, state)); });
  const std::array extra{
      textures[0], textures[1], platform::StartupPictureTextureHandle{8, 88, 808}};
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(submissions, resources, extra, state)); });
  const std::array swapped{
      platform::StartupPictureTextureHandle{2, 33, 402},
      platform::StartupPictureTextureHandle{4, 91, 701}};
  rejects([&] { static_cast<void>(platform::SdlStartupPictureFramePacket::assemble(submissions, resources, swapped, state)); });
  return 0;
}
