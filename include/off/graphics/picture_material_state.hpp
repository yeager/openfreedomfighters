#pragma once

#include <cstdint>
#include <optional>

namespace off::graphics {

enum class PictureStageOperation {
  select_argument_1,
  modulate_twice,
  add,
  disable
};
enum class PictureStageArgument { texture, diffuse };
enum class PictureBlendFactor {
  zero,
  one,
  source_alpha,
  inverse_source_alpha,
  source_color
};
enum class PictureDepthComparison { always, less_equal };
enum class PictureCullMode { none, clockwise };
enum class PictureAddressMode { wrap, clamp };

struct PictureStageRequests {
  std::optional<PictureStageOperation> rgb_operation;
  std::optional<PictureStageArgument> rgb_argument_1;
  std::optional<PictureStageArgument> rgb_argument_2;
  std::optional<PictureStageOperation> alpha_operation;
  std::optional<PictureStageArgument> alpha_argument_1;
  std::optional<PictureStageArgument> alpha_argument_2;
};

struct PictureResourceBindingRequests {
  // true means bind the caller's selected resource, not a texture identifier.
  std::optional<bool> bind_selected_texture;
  PictureStageRequests stage_zero;
};

struct PictureMaterialRequests {
  std::optional<bool> blend_enabled;
  std::optional<PictureBlendFactor> source_blend;
  std::optional<PictureBlendFactor> destination_blend;
  std::optional<bool> alpha_test_enabled;
  std::optional<bool> depth_write_enabled;
  std::optional<PictureDepthComparison> depth_comparison;
  std::optional<PictureCullMode> cull_mode;
  std::optional<PictureAddressMode> address_u;
  std::optional<PictureAddressMode> address_v;
  std::optional<std::uint32_t> texture_factor;
  PictureStageRequests stage_zero;
};

struct PictureMaterialCacheKey {
  std::uint32_t material_word{};
  std::uint32_t secondary_word{};
  std::uint32_t alpha_threshold{};
  bool operator==(const PictureMaterialCacheKey &) const = default;
};

struct PictureMaterialStateInput {
  // Must be an explicit draw-time input, not inferred from an authored record.
  std::uint32_t runtime_material_word;
  std::uint32_t pass_disable_mask_a;
  std::uint32_t pass_disable_mask_b;
  std::uint8_t suppression_byte;
  std::optional<PictureMaterialCacheKey> cached_key;
  std::uint32_t mode_selector;
  bool resource_transition;
};

struct PictureMaterialStateRequests {
  std::uint8_t effective_features{};
  // Apply resource requests first; subsequent material requests may override.
  PictureResourceBindingRequests resource_binding;
  PictureMaterialRequests material;
  // nullopt explicitly means leave the cache unchanged, not clear the cache.
  std::optional<PictureMaterialCacheKey> cache_replacement;
};

// Conditional CPU requests only. Omitted states remain inherited. The picture
// call uses secondary 0xffffffff and threshold zero. No GPU state is mutated.
[[nodiscard]] PictureMaterialStateRequests
resolve_picture_material_state(const PictureMaterialStateInput &input);

// Base-picture load-time mapping only, not a final resource-state derivation.
// Runtime aliases and subsequent writers are deliberately not resolved here.
[[nodiscard]] std::uint32_t
map_base_picture_material_property(std::uint32_t authored_property,
                                   bool add_material_bit_0x1 = false);

struct PictureAlphaMaterialTransition {
  std::uint32_t material_word{};
  bool material_changed{};
};

// Material-bit portion of the base-picture alpha setter only. Compare the
// whole incoming integer with 255; do not clamp or truncate it to a byte.
// Does not update alpha storage, propagate resource writes or schedule a fade.
[[nodiscard]] PictureAlphaMaterialTransition update_picture_alpha_material(
    std::uint32_t material_word, std::uint32_t alpha_input) noexcept;

} // namespace off::graphics
