#include "off/platform/intro_preview_diagnostic.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <unordered_set>

namespace off::platform {
namespace {

[[nodiscard]] bool supported_color_format(SDL_GPUTextureFormat format) {
  return format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM ||
         format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
}

[[nodiscard]] SdlIntroDrawState baseline_state(SDL_GPUTextureFormat format) {
  SdlIntroDrawState result{};
  result.sampler.min_filter = SDL_GPU_FILTER_LINEAR;
  result.sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  result.sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  result.sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  result.sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  result.sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  result.rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
  result.rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
  result.rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  result.blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  result.blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  result.blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  result.blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  result.blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
  result.blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  result.blend.enable_blend = true;
  result.color_format = format;
  return result;
}

[[nodiscard]] std::array<float, 16> fit_projection(
    std::span<const data::BoundPictureDrawGroup> groups, std::uint32_t width,
    std::uint32_t height) {
  double left = std::numeric_limits<double>::infinity();
  double right = -std::numeric_limits<double>::infinity();
  double bottom = std::numeric_limits<double>::infinity();
  double top = -std::numeric_limits<double>::infinity();
  for (const auto &group : groups) {
    for (const auto &quad : group.quads) {
      const std::array values{quad.local_center_x, quad.local_center_y,
                              quad.horizontal_edge_span,
                              quad.vertical_edge_span};
      if (!std::ranges::all_of(values,
                               [](float value) { return std::isfinite(value); }) ||
          quad.horizontal_edge_span < 0.0F || quad.vertical_edge_span < 0.0F)
        throw std::runtime_error("intro diagnostic picture geometry is invalid");
      const auto half_width = static_cast<double>(quad.horizontal_edge_span) * 0.5;
      const auto half_height = static_cast<double>(quad.vertical_edge_span) * 0.5;
      left = std::min(left, static_cast<double>(quad.local_center_x) - half_width);
      right = std::max(right, static_cast<double>(quad.local_center_x) + half_width);
      bottom = std::min(bottom, static_cast<double>(quad.local_center_y) - half_height);
      top = std::max(top, static_cast<double>(quad.local_center_y) + half_height);
    }
  }
  const auto span_x = right - left;
  const auto span_y = top - bottom;
  if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(bottom) ||
      !std::isfinite(top) || span_x <= 0.0 || span_y <= 0.0)
    throw std::runtime_error("intro diagnostic picture has no finite extent");
  const auto aspect = static_cast<double>(width) / static_cast<double>(height);
  const auto half_height = std::max(span_y * 0.5, span_x / (2.0 * aspect)) * 1.05;
  const auto scale_x = 1.0 / (half_height * aspect);
  const auto scale_y = 1.0 / half_height;
  const auto center_x = (left + right) * 0.5;
  const auto center_y = (bottom + top) * 0.5;
  const std::array<double, 4> values{scale_x, scale_y, -center_x * scale_x,
                                     -center_y * scale_y};
  if (!std::ranges::all_of(values,
                           [](double value) { return std::isfinite(value) &&
                                                       std::abs(value) <=
                                                           std::numeric_limits<float>::max(); }))
    throw std::runtime_error("intro diagnostic projection is invalid");
  return {static_cast<float>(scale_x), 0, 0, 0,
          0, static_cast<float>(scale_y), 0, 0,
          0, 0, 0, 0,
          static_cast<float>(-center_x * scale_x),
          static_cast<float>(-center_y * scale_y), 0, 1};
}

void validate_images(const graphics::IntroPreviewSnapshot &snapshot) {
  std::unordered_set<std::size_t> image_indices;
  for (const auto &image : snapshot.images) {
    const auto expected = static_cast<std::uint64_t>(image.mip_zero.width) *
                          image.mip_zero.height * 4U;
    if (image.mip_zero.width == 0U || image.mip_zero.height == 0U ||
        expected != image.mip_zero.pixels.size() ||
        !image_indices.insert(image.catalog_image_index).second)
      throw std::runtime_error("intro diagnostic snapshot image is invalid");
  }
  for (const auto &group : snapshot.draw.draw_plan.groups())
    if (!image_indices.contains(group.texture.image_index))
      throw std::runtime_error("intro diagnostic snapshot is missing a picture image");
}

} // namespace

IntroPreviewDiagnosticSubmission IntroPreviewDiagnosticSubmission::build(
    const graphics::IntroPreviewSnapshot &snapshot, std::uint32_t width,
    std::uint32_t height, SDL_GPUTextureFormat color_format) {
  if (width == 0U || height == 0U || !supported_color_format(color_format) ||
      snapshot.draw.draw_plan.groups().empty())
    throw std::runtime_error("intro diagnostic submission inputs are invalid");
  validate_images(snapshot);
  const auto projection =
      fit_projection(snapshot.draw.draw_plan.groups(), width, height);
  const IntroPictureSubmissionInput input{
      .groups = snapshot.draw.draw_plan.groups(),
      .transform = {.basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}},
      .projection = projection,
      .viewport = {0, 0, static_cast<float>(width), static_cast<float>(height),
                   0.0F, 1.0F},
      .scissor = {0, 0, static_cast<int>(width), static_cast<int>(height)},
      .stage = {{}, graphics::PictureStageOperation::select_argument_1,
                graphics::PictureStageArgument::texture,
                graphics::PictureStageArgument::diffuse,
                graphics::PictureStageOperation::select_argument_1,
                graphics::PictureStageArgument::texture,
                graphics::PictureStageArgument::diffuse},
      .packed_texture_factor = 0xffffffffU,
      .state = baseline_state(color_format)};
  return IntroPreviewDiagnosticSubmission(IntroPictureSubmission::assemble(input));
}

} // namespace off::platform
