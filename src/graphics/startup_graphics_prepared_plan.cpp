#include "off/graphics/startup_graphics_prepared_plan.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <ranges>
#include <stdexcept>

namespace off::graphics {
namespace {

[[nodiscard]] bool finite(const data::PictureQuad &quad) {
  const std::array values{quad.local_x_min, quad.local_x_max, quad.local_y_min,
                          quad.local_y_max, quad.local_z, quad.u_min,
                          quad.u_max, quad.v_min, quad.v_max,
                          quad.local_center_x, quad.local_center_y,
                          quad.horizontal_edge_span, quad.vertical_edge_span};
  return std::ranges::all_of(values,
                             [](float value) { return std::isfinite(value); });
}

} // namespace

StartupGraphicsPreparedPlan
prepare_startup_graphics_plan(const StartupGraphicsAsset &asset,
                              std::uint8_t requested_state) {
  StartupGraphicsPreparedPlan result;
  const auto traversal =
      asset.composition().traversal_emission_plan(requested_state);
  result.requested_state_ = traversal.requested_state;
  result.effective_state_ = traversal.effective_state;

  result.resources_.reserve(asset.images().size());
  for (std::size_t index = 0; index < asset.images().size(); ++index) {
    const auto &image = asset.images()[index];
    if (image.mip_zero.width == 0 || image.mip_zero.height == 0)
      throw std::runtime_error("startup prepared resource has zero extent");
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (asset.images()[previous].catalog_image_index ==
              image.catalog_image_index ||
          asset.images()[previous].texture_id == image.texture_id)
        throw std::runtime_error(
            "startup prepared resource identity is duplicated");
    }
    result.resources_.push_back({index, image.catalog_image_index,
                                 image.texture_id, image.mip_zero.width,
                                 image.mip_zero.height});
  }
  if (result.resources_.empty() ||
      result.resources_.size() > maximum_startup_graphics_images)
    throw std::runtime_error("startup prepared resource set is empty or too large");
  if (traversal.pictures.size() > maximum_startup_prepared_pictures ||
      traversal.draw_emissions.size() > maximum_startup_prepared_submissions)
    throw std::runtime_error("startup prepared traversal exceeds its bounds");

  result.pictures_.reserve(traversal.pictures.size());
  result.quads_.reserve(traversal.draw_emissions.size());
  result.submissions_.reserve(traversal.draw_emissions.size());
  for (std::size_t picture_ordinal = 0;
       picture_ordinal < traversal.pictures.size(); ++picture_ordinal) {
    const auto &picture = traversal.pictures[picture_ordinal];
    if (picture.row_index >= asset.composition().rows().size() ||
        picture.picture_index >=
            asset.composition().rows()[picture.row_index].pictures.size() ||
        picture.first_draw_emission != result.submissions_.size() ||
        picture.draw_emission_count > traversal.draw_emissions.size() -
                                          picture.first_draw_emission)
      throw std::runtime_error("startup prepared picture span is invalid");
    const auto &source_picture =
        asset.composition().rows()[picture.row_index]
            .pictures[picture.picture_index];
    const auto &source_row = asset.composition().rows()[picture.row_index];
    if (picture.row_directory_index != source_row.owner_directory_index ||
        picture.picture_directory_index != source_picture.directory_index ||
        picture.role != source_picture.role ||
        picture.draw_emission_count != std::accumulate(
            source_picture.draw_plan.groups().begin(),
            source_picture.draw_plan.groups().end(), std::size_t{0},
            [](std::size_t total, const auto& group) {
              return total + group.quads.size();
            }))
      throw std::runtime_error("startup prepared picture identity is invalid");
    if (source_picture.alignment_enum > 15U ||
        (source_picture.extension_control.has_value() &&
         *source_picture.extension_control > 16U))
      throw std::runtime_error("startup prepared picture control is invalid");
    for (const auto &previous : result.pictures_)
      if (previous.picture_directory_index == picture.picture_directory_index)
        throw std::runtime_error(
            "startup prepared picture identity is duplicated");

    result.pictures_.push_back(
        {picture.row_index, picture.picture_index,
         picture.row_directory_index, picture.picture_directory_index,
         picture.role, source_picture.base_render_property,
         source_picture.authored_alpha, source_picture.alignment_enum,
         source_picture.extension_control, picture.first_draw_emission,
         picture.draw_emission_count});
    std::size_t relative = 0;
    for (std::size_t group_index = 0;
         group_index < source_picture.draw_plan.groups().size(); ++group_index) {
      const auto &group = source_picture.draw_plan.groups()[group_index];
      for (const auto &quad : group.quads) {
        const auto emission_ordinal = picture.first_draw_emission + relative;
        const auto &emission = traversal.draw_emissions[emission_ordinal];
        if (emission.row_index != picture.row_index ||
            emission.picture_index != picture.picture_index ||
            emission.picture_directory_index != picture.picture_directory_index ||
            emission.group_index != group_index ||
            emission.descriptor_index != quad.descriptor_index || !finite(quad))
          throw std::runtime_error(
              "startup prepared emission order disagrees with source descriptors");
        const auto resource = std::ranges::find_if(
            result.resources_, [&](const auto &candidate) {
              return candidate.catalog_image_index == group.texture.image_index &&
                     candidate.texture_id == group.texture.texture_id;
            });
        if (resource == result.resources_.end())
          throw std::runtime_error("startup prepared texture identity is missing");
        const auto prepared_quad = result.quads_.size();
        result.quads_.push_back(
            {emission_ordinal, emission.row_index, emission.picture_index,
             emission.picture_directory_index, group_index,
             quad.descriptor_index, resource->resource_index,
             quad, group.texture.prm_offset,
             group.texture.authored_texture_resource_record});
        result.submissions_.push_back(
            {emission_ordinal, picture_ordinal, prepared_quad});
        ++relative;
      }
    }
    if (relative != picture.draw_emission_count)
      throw std::runtime_error("startup prepared descriptor span is incomplete");
  }
  if (result.pictures_.size() > maximum_startup_prepared_pictures ||
      result.quads_.size() > maximum_startup_prepared_submissions ||
      result.submissions_.size() != result.quads_.size() ||
      result.submissions_.size() != traversal.draw_emissions.size())
    throw std::runtime_error("startup prepared plan exceeds its bounds");
  return result;
}

} // namespace off::graphics
