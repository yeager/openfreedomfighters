#pragma once

#include "off/graphics/picture_view_transition.hpp"

#include <array>
#include <variant>

namespace off::graphics {

struct PictureTrackedStage {
  std::optional<std::uint64_t> texture;
  PictureStageOperation rgb_operation;
  PictureStageArgument rgb_argument_1, rgb_argument_2;
  PictureStageOperation alpha_operation;
  PictureStageArgument alpha_argument_1, alpha_argument_2;
};

// One live renderer context shared by reset, diagnostics, views and materials.
// Aggregate initialization is not a claim about inherited backend state.
struct PictureDrawContext {
  PictureViewFogState fog;
  PictureMaterialCacheKey material_cache;
  std::array<float, 16> projection_cache, world_cache, view_cache;
  std::array<PictureTrackedStage, 8> stages;
  PictureBlendFactor source_blend, destination_blend;
  bool blend_enabled, depth_write_enabled, depth_write_suppressed;
  PictureDepthComparison depth_comparison, alpha_comparison;
  bool alpha_test_enabled, lighting_enabled;
  PictureCullMode cull_mode;
  std::array<std::optional<std::uint64_t>, 2> streams;
  std::array<std::uint32_t, 2> strides;
  std::optional<std::uint64_t> indices;
  std::uint32_t index_base_vertex, vertex_format;
  std::optional<std::uint64_t> pixel_shader;
  // Inherited controls: reset does not overwrite these four fields.
  std::uint32_t disable_mask_a, disable_mask_b, material_mode;
  std::uint8_t material_suppression;
  std::uint8_t effective_features;
};

enum class PictureResetOperation {
  source_blend, destination_blend, blend_enabled, depth_write_enabled,
  depth_comparison, alpha_comparison, alpha_test_enabled, fog_enabled,
  fog_color, fog_start, fog_end, lighting_enabled, cull_mode,
  null_texture, rgb_operation, rgb_argument_1, rgb_argument_2,
  alpha_operation, alpha_argument_1, alpha_argument_2,
  null_stream_zero, null_indices, vertex_format, null_pixel_shader,
  wireframe_fill
};
using PictureResetValue = std::variant<std::monostate, bool, std::uint32_t, float,
    PictureBlendFactor, PictureDepthComparison, PictureCullMode,
    PictureStageOperation, PictureStageArgument>;
struct PictureResetCommand {
  PictureResetOperation operation;
  // Only texture/stage operations use stage. Other commands carry zero.
  std::uint32_t stage;
  // null_stream_zero carries zero stride; null_indices carries RETAINED base
  // vertex. Null texture/pixel shader and wireframe have monostate payloads.
  PictureResetValue value;
};
struct PictureDrawResetHooks {
  // Backend sink must preserve context/lifetimes. Throws retain their prefix.
  std::function<void(const PictureResetCommand&)> submit;
  // Both getter and returned diagnostic can mutate the shared context. The
  // zero-feature branch captures feature bits before either callback runs.
  std::function<std::function<void(std::uint8_t)>()> diagnostic_service;
};

class PictureDrawReset final {
public:
  PictureDrawReset() = default;
  PictureDrawReset(const PictureDrawReset&) = delete;
  PictureDrawReset& operator=(const PictureDrawReset&) = delete;
  PictureDrawReset(PictureDrawReset&&) = delete;
  PictureDrawReset& operator=(PictureDrawReset&&) = delete;

  // Effective MaxTextureBlendStages after configuration, explicitly supplied;
  // 0..8 supported as native bounds policy, never clamped or guessed.
  // Invoke through each ordered draw reset hook, including empty rounds.
  // Reentry/incomplete hooks/invalid count reject before effects. Exceptions
  // otherwise retain their prefix; abort the frame rather than roll back.
  void run(PictureDrawContext& context, std::uint32_t effective_stage_count,
           const PictureDrawResetHooks& hooks);
private:
  bool running_{false};
};

} // namespace off::graphics
