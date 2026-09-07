#include "off/graphics/intro_preview_builder.hpp"

#include "off/graphics/intro_runtime.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace off::graphics {
namespace {

constexpr std::uint32_t picture_source_type = 0x00200046U;

void validate_image(const IntroPreparedImage &image) {
  if (image.mip_zero.width == 0 || image.mip_zero.height == 0)
    throw std::runtime_error("intro preview image has zero dimensions");
  const auto width = static_cast<std::uint64_t>(image.mip_zero.width);
  const auto height = static_cast<std::uint64_t>(image.mip_zero.height);
  if (width > std::numeric_limits<std::uint64_t>::max() / height / 4U)
    throw std::runtime_error("intro preview image dimensions overflow");
  const auto expected = width * height * 4U;
  if (expected != image.mip_zero.pixels.size())
    throw std::runtime_error("intro preview image pixels are invalid");
}

} // namespace

IntroPreviewSnapshot build_intro_preview(const IntroRuntime &runtime,
                                         std::size_t source_index,
                                         IntroPreviewTarget target,
                                         IntroPreviewPolicy policy) {
  if (policy != IntroPreviewPolicy::exact_source_picture)
    throw std::runtime_error("intro preview policy is unsupported");
  if (target.width == 0 || target.height == 0)
    throw std::runtime_error("intro preview target dimensions are invalid");

  const auto &directory = runtime.resources().sources().directory();
  if (source_index >= directory.size())
    throw std::runtime_error("intro preview source is unknown");
  if (directory[source_index].source_type != picture_source_type)
    throw std::runtime_error("intro preview source is not a supported picture");

  const auto &picture = runtime.picture_for_source(source_index);
  const auto plan = picture.draw_plan();
  if (plan.groups().empty())
    throw std::runtime_error("intro preview picture has no drawable groups");

  std::unordered_set<std::size_t> needed;
  for (const auto &group : plan.groups()) {
    if (group.quads.empty())
      throw std::runtime_error("intro preview picture has an empty draw group");
    if (!needed.insert(group.texture.image_index).second)
      continue;
  }

  IntroPreviewSnapshot result{target, {source_index, plan}, {}};
  result.images.reserve(needed.size());
  std::unordered_set<std::size_t> supplied;
  for (const auto &image : runtime.resources().images()) {
    if (!needed.contains(image.catalog_image_index))
      continue;
    if (!supplied.insert(image.catalog_image_index).second)
      throw std::runtime_error("intro preview has duplicate image identity");
    validate_image(image);
    result.images.push_back(image);
  }
  if (result.images.size() != needed.size())
    throw std::runtime_error("intro preview references an absent image");
  std::ranges::sort(result.images, {}, &IntroPreparedImage::catalog_image_index);
  return result;
}

} // namespace off::graphics
