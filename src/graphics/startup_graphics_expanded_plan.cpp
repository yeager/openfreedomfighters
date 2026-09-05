#include "off/graphics/startup_graphics_expanded_plan.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace off::graphics {

StartupGraphicsExpandedPlan expand_startup_graphics_plan(
    const StartupGraphicsPreparedPlan &prepared,
    std::span<const StartupGraphicsPictureTransform> transforms) {
  if (prepared.pictures().size() > maximum_startup_prepared_pictures ||
      prepared.submissions().size() > maximum_startup_prepared_submissions ||
      prepared.quads().size() != prepared.submissions().size() ||
      prepared.resources().size() != startup_graphics_image_count ||
      transforms.size() != prepared.pictures().size())
    throw std::runtime_error("Startup expansion input counts are invalid");
  // The prepared-plan factory guarantees unique picture directory identities.
  // Equal counts, known keys and unique transform keys establish a bijection.
  for (std::size_t i = 0; i < transforms.size(); ++i) {
    const auto &entry = transforms[i];
    if (std::none_of(prepared.pictures().begin(), prepared.pictures().end(),
                     [&](const auto &picture) {
                       return picture.picture_directory_index ==
                              entry.picture_directory_index;
                     }))
      throw std::runtime_error(
          "Startup expansion transform has unknown picture identity");
    for (std::size_t earlier = 0; earlier < i; ++earlier)
      if (transforms[earlier].picture_directory_index ==
          entry.picture_directory_index)
        throw std::runtime_error(
            "Startup expansion transform identity is duplicated");
    for (float value : entry.transform.basis)
      if (!std::isfinite(value))
        throw std::runtime_error("Startup expansion transform is non-finite");
    for (float value : entry.transform.translation)
      if (!std::isfinite(value))
        throw std::runtime_error("Startup expansion transform is non-finite");
  }
  StartupGraphicsExpandedPlan result;
  result.requested_state_ = prepared.requested_state();
  result.effective_state_ = prepared.effective_state();
  result.resources_ = prepared.resources();
  result.pictures_ = prepared.pictures();
  result.submissions_.reserve(prepared.submissions().size());
  for (const auto &submission : prepared.submissions()) {
    if (submission.prepared_picture_index >= prepared.pictures().size() ||
        submission.prepared_quad_index >= prepared.quads().size())
      throw std::runtime_error("Startup expansion submission index is invalid");
    const auto &picture =
        prepared.pictures()[submission.prepared_picture_index];
    const auto &quad = prepared.quads()[submission.prepared_quad_index];
    if (quad.picture_directory_index != picture.picture_directory_index ||
        quad.emission_ordinal != submission.emission_ordinal ||
        quad.resource_index >= result.resources_.size())
      throw std::runtime_error(
          "Startup expansion submission identity is invalid");
    const auto transform = std::find_if(
        transforms.begin(), transforms.end(), [&](const auto &entry) {
          return entry.picture_directory_index ==
                 picture.picture_directory_index;
        });
    if (transform == transforms.end())
      throw std::runtime_error(
          "Startup expansion picture transform is missing");
    const auto expanded = expand_picture_descriptors(std::span(&quad.source, 1),
                                                     transform->transform);
    StartupGraphicsExpandedSubmission output{
        .emission_ordinal = submission.emission_ordinal,
        .prepared_picture_index = submission.prepared_picture_index,
        .picture_directory_index = quad.picture_directory_index,
        .row_index = quad.row_index,
        .picture_index = quad.picture_index,
        .group_index = quad.group_index,
        .descriptor_index = quad.descriptor_index,
        .resource_index = quad.resource_index,
        .vertices = {},
        .indices = {}};
    std::copy(expanded.front().vertices.begin(),
              expanded.front().vertices.end(), output.vertices.begin());
    std::copy(expanded.front().indices.begin(), expanded.front().indices.end(),
              output.indices.begin());
    result.submissions_.push_back(output);
  }
  return result;
}

} // namespace off::graphics
