#include "off/graphics/picture_material_state.hpp"

#include <array>
#include <limits>

namespace off::graphics {

PictureAlphaMaterialTransition update_picture_alpha_material(
    std::uint32_t material_word, std::uint32_t alpha_input) noexcept {
  const auto updated = alpha_input == 255U ? material_word & ~1U
                                         : material_word | 1U;
  return {updated, updated != material_word};
}

std::uint32_t
map_base_picture_material_property(std::uint32_t authored_property,
                                   bool add_material_bit_0x1) {
  constexpr std::array<std::uint32_t, 7> mapping{
      0x60010, 0x60012, 0x60014, 0x60011, 0x60018, 0x60210, 0x60211};
  const auto mapped = authored_property < mapping.size()
                          ? mapping[authored_property]
                          : authored_property;
  return mapped | (add_material_bit_0x1 ? 1U : 0U);
}

PictureMaterialStateRequests
resolve_picture_material_state(const PictureMaterialStateInput &input) {
  PictureMaterialStateRequests result;
  result.effective_features = static_cast<std::uint8_t>(
      ~(input.pass_disable_mask_a | input.pass_disable_mask_b) & 3U);
  if (input.resource_transition && (result.effective_features & 1U) != 0) {
    result.resource_binding.bind_selected_texture = true;
    auto &stage = result.resource_binding.stage_zero;
    stage.rgb_argument_1 = PictureStageArgument::texture;
    stage.alpha_argument_1 = PictureStageArgument::texture;
    const bool modulate = (result.effective_features & 2U) != 0;
    stage.rgb_operation = modulate ? PictureStageOperation::modulate_twice
                                   : PictureStageOperation::select_argument_1;
    stage.alpha_operation = stage.rgb_operation;
    if (modulate) {
      stage.rgb_argument_2 = PictureStageArgument::diffuse;
      stage.alpha_argument_2 = PictureStageArgument::diffuse;
    }
  }

  if (input.suppression_byte != 0)
    return result;
  const PictureMaterialCacheKey requested{
      input.runtime_material_word, std::numeric_limits<std::uint32_t>::max(),
      0};
  if (input.cached_key == requested)
    return result;
  result.cache_replacement = requested;
  auto &material = result.material;
  if (result.effective_features == 0) {
    if (input.mode_selector == 1)
      material.texture_factor = requested.secondary_word;
    return result;
  }

  const auto word = input.runtime_material_word;
  material.blend_enabled = (word & 0x402607U) != 0;
  material.alpha_test_enabled = false;
  if (*material.blend_enabled) {
    if ((word & 0x2000U) != 0) {
      material.source_blend = PictureBlendFactor::zero;
      material.destination_blend = PictureBlendFactor::source_color;
      material.stage_zero.rgb_operation = PictureStageOperation::add;
      material.stage_zero.rgb_argument_1 = PictureStageArgument::texture;
      material.stage_zero.rgb_argument_2 = PictureStageArgument::diffuse;
      material.stage_zero.alpha_operation = PictureStageOperation::disable;
      // This is an ordinary stored key value, not cache invalidation.
      result.cache_replacement->material_word =
          std::numeric_limits<std::uint32_t>::max();
    } else {
      material.source_blend = PictureBlendFactor::source_alpha;
      material.destination_blend =
          (word & 2U) != 0 && (word & 0x400U) == 0
              ? PictureBlendFactor::one
              : PictureBlendFactor::inverse_source_alpha;
    }
  }
  material.depth_write_enabled = (word & 0x40000U) == 0;
  material.depth_comparison = (word & 0x20000U) != 0
                                  ? PictureDepthComparison::always
                                  : PictureDepthComparison::less_equal;
  material.cull_mode = (word & 0x80000U) != 0 ? PictureCullMode::none
                                              : PictureCullMode::clockwise;
  material.address_u = (word & 0x4000U) != 0 ? PictureAddressMode::clamp
                                             : PictureAddressMode::wrap;
  material.address_v = (word & 0x8000U) != 0 ? PictureAddressMode::clamp
                                             : PictureAddressMode::wrap;
  return result;
}

} // namespace off::graphics
