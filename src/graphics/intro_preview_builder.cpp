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
  if (policy != IntroPreviewPolicy::exact_source_picture &&
      policy != IntroPreviewPolicy::admitted_first_cut_legal_picture &&
      policy != IntroPreviewPolicy::admitted_first_cut_fade_picture)
    throw std::runtime_error("intro preview policy is unsupported");
  if (target.width == 0 || target.height == 0)
    throw std::runtime_error("intro preview target dimensions are invalid");

  const auto &directory = runtime.resources().sources().directory();
  if (source_index >= directory.size())
    throw std::runtime_error("intro preview source is unknown");
  if (directory[source_index].source_type != picture_source_type)
    throw std::runtime_error("intro preview source is not a supported picture");

  if (policy == IntroPreviewPolicy::admitted_first_cut_legal_picture) {
    const auto *owner_receipt = runtime.legal_picture_reader_state();
    const auto *component_receipt =
        runtime.legal_picture_component_reader_state();
    if (!owner_receipt || !component_receipt ||
        component_receipt->source_directory_index != source_index ||
        owner_receipt->owner != runtime.source_handle(source_index) ||
        component_receipt->owner != owner_receipt->owner ||
        component_receipt->resource != owner_receipt->resource ||
        component_receipt->component_index != owner_receipt->component_index ||
        component_receipt->picture_asset_reference !=
            owner_receipt->picture_asset_reference)
      throw std::runtime_error(
          "intro preview first-cut legal-picture admission is unavailable");
  }
  if (policy == IntroPreviewPolicy::admitted_first_cut_fade_picture) {
    const auto owner_receipt =
        runtime.fade_picture_reader_states().find(source_index);
    const auto component_receipt =
        runtime.fade_picture_component_reader_states().find(source_index);
    if (owner_receipt == runtime.fade_picture_reader_states().end() ||
        component_receipt ==
            runtime.fade_picture_component_reader_states().end() ||
        owner_receipt->second.owner != runtime.source_handle(source_index) ||
        component_receipt->second.owner != owner_receipt->second.owner ||
        component_receipt->second.resource != owner_receipt->second.resource ||
        component_receipt->second.component_index !=
            owner_receipt->second.component_index ||
        component_receipt->second.picture_asset_reference !=
            owner_receipt->second.picture_asset_reference)
      throw std::runtime_error(
          "intro preview first-cut FadeToBlack admission is unavailable");
  }

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

IntroPreviewSnapshot build_incomplete_intro_fallback(
    const IntroRuntime &runtime, IntroPreviewTarget target) {
  const auto *const receipt = runtime.legal_picture_component_reader_state();
  if (receipt == nullptr)
    throw std::runtime_error(
        "incomplete intro fallback requires a legal-picture reader receipt");
  return build_intro_preview(
      runtime, receipt->source_directory_index, target,
      IntroPreviewPolicy::admitted_first_cut_legal_picture);
}

std::vector<IntroPreviewSnapshot> build_admitted_first_cut_fade_previews(
    const IntroRuntime &runtime, IntroPreviewTarget target) {
  const auto &owners = runtime.fade_picture_reader_states();
  const auto &components = runtime.fade_picture_component_reader_states();
  const auto &sources = runtime.resources().sources();
  const auto &directory = sources.directory();
  std::vector<std::size_t> expected_sources;
  for (const auto &command : runtime.resources().first_cut().commands) {
    const auto source =
        sources.local_source_for_authored_reference(command.target_reference);
    if (!source || *source >= directory.size())
      throw std::runtime_error(
          "intro preview first-cut command target is unresolved");
    const auto &row = directory[*source];
    if (row.source_type == picture_source_type && row.attachments.size() == 1U &&
        row.attachments[0].parameter == 0.0F &&
        sources.attachment_identifier(*source, 0U) == "ZWINPIC_FadeToBlack")
      expected_sources.push_back(*source);
  }
  std::ranges::sort(expected_sources);
  expected_sources.erase(
      std::unique(expected_sources.begin(), expected_sources.end()),
      expected_sources.end());
  // A partial result would create an invented selection policy, so do not
  // expose anything until every exact command-target reader/component pair
  // agrees.  This validates the retained package's actual target population,
  // rather than assigning a hard-coded count to another supported package.
  if (expected_sources.empty() || owners.size() != expected_sources.size() ||
      components.size() != owners.size())
    throw std::runtime_error(
        "intro preview first-cut FadeToBlack receipt set is incomplete");
  std::vector<IntroPreviewSnapshot> result;
  result.reserve(owners.size());
  for (const auto &[source, owner] : owners) {
    if (!std::ranges::binary_search(expected_sources, source))
      throw std::runtime_error(
          "intro preview first-cut FadeToBlack receipt source is unexpected");
    const auto component = components.find(source);
    if (component == components.end() ||
        component->second.owner != owner.owner ||
        component->second.resource != owner.resource ||
        component->second.component_index != owner.component_index ||
        component->second.picture_asset_reference != owner.picture_asset_reference)
      throw std::runtime_error(
          "intro preview first-cut FadeToBlack receipt set is inconsistent");
    result.push_back(build_intro_preview(
        runtime, source, target,
        IntroPreviewPolicy::admitted_first_cut_fade_picture));
  }
  return result;
}

std::vector<FirstCutPicturePreviewStep>
admitted_first_cut_picture_preview_steps(const IntroRuntime &runtime) {
  const auto &sources = runtime.resources().sources();
  const auto &directory = sources.directory();
  const auto *const legal = runtime.legal_picture_component_reader_state();
  const auto &fades = runtime.fade_picture_component_reader_states();
  std::vector<FirstCutPicturePreviewStep> result;
  const auto &commands = runtime.resources().first_cut().commands;
  result.reserve(commands.size());
  for (std::size_t command_index = 0; command_index < commands.size();
       ++command_index) {
    const auto source =
        sources.local_source_for_authored_reference(
            commands[command_index].target_reference);
    if (!source || *source >= directory.size() ||
        directory[*source].source_type != picture_source_type)
      continue;
    if (legal != nullptr && legal->source_directory_index == *source) {
      result.push_back({command_index, *source,
                        IntroPreviewPolicy::admitted_first_cut_legal_picture});
      continue;
    }
    if (fades.contains(*source))
      result.push_back({command_index, *source,
                        IntroPreviewPolicy::admitted_first_cut_fade_picture});
  }
  return result;
}

IntroPreviewSnapshot build_admitted_first_cut_picture_step(
    const IntroRuntime &runtime, std::size_t command_index,
    IntroPreviewTarget target) {
  const auto steps = admitted_first_cut_picture_preview_steps(runtime);
  const auto found = std::ranges::find(
      steps, command_index, &FirstCutPicturePreviewStep::command_index);
  if (found == steps.end())
    throw std::runtime_error(
        "first-cut diagnostic command does not select an admitted picture");
  return build_intro_preview(runtime, found->source_index, target,
                             found->policy);
}

std::span<const IntroPreparedImage> select_intro_gpu_upload_images(
    std::span<const IntroPreparedImage> retained_images,
    const IntroPreviewSnapshot *explicit_diagnostic) noexcept {
  if (explicit_diagnostic != nullptr)
    return explicit_diagnostic->images;
  return retained_images;
}

} // namespace off::graphics
