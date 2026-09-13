#include "off/platform/intro_preview_diagnostic.hpp"
#include "off/graphics/intro_preview_builder.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <class F> void rejects(F &&action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "expected diagnostic submission rejection");
}
} // namespace

int main() {
  using namespace off;
  const std::array retained_images{
      graphics::IntroPreparedImage{.catalog_image_index = 3U},
      graphics::IntroPreparedImage{.catalog_image_index = 7U},
      graphics::IntroPreparedImage{.catalog_image_index = 11U}};
  const std::array descriptors{data::PictureResourceDescriptor{
      .local_center_x = 20.0F, .local_center_y = -10.0F,
      .u_max = 1.0F, .v_min = 1.0F,
      .horizontal_edge_span = 40.0F, .vertical_edge_span = 20.0F,
      .modulation_color = 0xffffffffU}};
  const std::array groups{data::PictureDrawGroup{.descriptor_span_count = 1U,
                                                   .first_descriptor_index = 0U}};
  const std::array textures{data::PictureTextureBinding{.texture_id = 1U,
                                                         .image_index = 7U}};
  graphics::IntroPreviewSnapshot snapshot{
      {1280U, 720U}, {1U, data::PictureDrawPlan::build(descriptors, groups, textures)},
      {{{7U, 1U, {1U, 1U, {255U, 255U, 255U, 255U}}}}}};
  const auto normal_uploads = graphics::select_intro_gpu_upload_images(
      retained_images, nullptr);
  const auto diagnostic_uploads = graphics::select_intro_gpu_upload_images(
      retained_images, &snapshot);
  check(normal_uploads.size() == retained_images.size() &&
            normal_uploads.data() == retained_images.data(),
        "normal retained intro uploads every prepared image");
  check(diagnostic_uploads.size() == snapshot.images.size() &&
            diagnostic_uploads.data() == snapshot.images.data() &&
            diagnostic_uploads.front().catalog_image_index == 7U,
        "explicit diagnostic uploads only its snapshot-required image");
  // Construct directly from a hand-written plan so the test carries no retail data.
  const auto diagnostic = platform::IntroPreviewDiagnosticSubmission::build(
      snapshot, 1280U, 720U, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
  const auto draws = diagnostic.draws();
  check(draws.size() == 1U && draws.front().catalog_image_index == 7U &&
            draws.front().viewport.w == 1280.0F &&
            draws.front().state.blend.enable_blend,
        "diagnostic keeps source image identity and establishes explicit baseline state");
  check(draws.front().projection[0] > 0.0F && draws.front().projection[5] > 0.0F &&
            draws.front().projection[12] < 0.0F &&
            draws.front().projection[13] > 0.0F,
        "diagnostic fit projection centers finite source geometry");
  rejects([&] {
    static_cast<void>(platform::IntroPreviewDiagnosticSubmission::build(
        snapshot, 0U, 720U, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM));
  });
  auto missing = snapshot;
  missing.images.clear();
  rejects([&] {
    static_cast<void>(platform::IntroPreviewDiagnosticSubmission::build(
        missing, 1280U, 720U, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM));
  });
  rejects([&] {
    static_cast<void>(platform::IntroPreviewDiagnosticSubmission::build(
        snapshot, 1280U, 720U, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT));
  });
  return 0;
}
