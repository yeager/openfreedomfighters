#include "off/platform/intro_picture_submission.hpp"

#include <stdexcept>

namespace off::platform {

IntroPictureSubmission IntroPictureSubmission::assemble(
    const IntroPictureSubmissionInput &input) {
  if (input.groups.empty())
    throw std::runtime_error("intro picture submission requires draw groups");
  IntroPictureSubmission result;
  result.batches_.reserve(input.groups.size());
  result.draws_.reserve(input.groups.size());
  for (const auto &group : input.groups) {
    if (group.quads.empty())
      throw std::runtime_error("intro picture submission has an empty draw group");
    result.batches_.push_back(
        graphics::expand_picture_descriptors(group.quads, input.transform));
    if (result.batches_.back().empty())
      throw std::runtime_error("intro picture submission has no expanded batches");
    result.draws_.push_back({.batches = result.batches_.back(),
                             .catalog_image_index = group.texture.image_index,
                             .projection = input.projection,
                             .viewport = input.viewport,
                             .scissor = input.scissor,
                             .stage = input.stage,
                             .packed_texture_factor = input.packed_texture_factor,
                             .state = input.state});
  }
  return result;
}

} // namespace off::platform
