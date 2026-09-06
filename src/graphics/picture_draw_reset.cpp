#include "off/graphics/picture_draw_reset.hpp"

#include <bit>
#include <stdexcept>

namespace off::graphics {
void PictureDrawReset::run(PictureDrawContext& c, std::uint32_t count,
                          const PictureDrawResetHooks& hooks) {
  if (running_ || count > c.stages.size() || !hooks.submit || !hooks.diagnostic_service)
    throw std::runtime_error("invalid or reentrant picture draw reset");
  struct Guard {
    bool& running;
    explicit Guard(bool& value) : running(value) { running = true; }
    ~Guard() { running = false; }
  } guard(running_);
  c.source_blend = PictureBlendFactor::one;
  c.destination_blend = PictureBlendFactor::zero;
  c.blend_enabled = false;
  c.depth_write_enabled = true;
  c.depth_comparison = PictureDepthComparison::less_equal;
  c.depth_write_suppressed = false;
  c.alpha_test_enabled = false;
  c.alpha_comparison = PictureDepthComparison::always;
  c.lighting_enabled = false;
  c.cull_mode = PictureCullMode::clockwise;
  c.fog.tracked_enabled = true;
  c.fog.colors.tracked_color = 0;
  c.fog.colors.base_color = 0;
  c.fog.colors.additive_color = 0xff000000U;
  c.fog.colors.special_color = 0xffffffffU;
  c.fog.start = std::bit_cast<float>(0x3f333333U);
  c.fog.end = std::bit_cast<float>(0x3f800000U);
  c.fog.suppression_latched = false;
  for (std::uint32_t i = 0; i < count; ++i)
    c.stages[i] = {std::nullopt, PictureStageOperation::disable,
        PictureStageArgument::texture, PictureStageArgument::current,
        PictureStageOperation::disable, PictureStageArgument::texture, PictureStageArgument::current};
  c.streams[0].reset(); c.strides[0] = 0;
  c.streams[1].reset(); c.strides[1] = 0;
  c.indices.reset(); // index_base_vertex deliberately survives.
  c.vertex_format = 0x142U;
  c.pixel_shader.reset();

  using Op = PictureResetOperation;
  const auto submit = [&](Op operation, PictureResetValue value = {}, std::uint32_t stage = 0) {
    hooks.submit({operation, stage, std::move(value)});
  };
  submit(Op::source_blend, c.source_blend);
  submit(Op::destination_blend, c.destination_blend);
  submit(Op::blend_enabled, c.blend_enabled);
  submit(Op::depth_write_enabled, c.depth_write_enabled);
  submit(Op::depth_comparison, c.depth_comparison);
  submit(Op::alpha_comparison, c.alpha_comparison);
  submit(Op::alpha_test_enabled, c.alpha_test_enabled);
  submit(Op::fog_enabled, c.fog.tracked_enabled);
  submit(Op::fog_color, c.fog.colors.tracked_color);
  submit(Op::fog_start, c.fog.start);
  submit(Op::fog_end, c.fog.end);
  submit(Op::lighting_enabled, c.lighting_enabled);
  submit(Op::cull_mode, c.cull_mode);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto& stage = c.stages[i];
    submit(Op::null_texture, {}, i);
    submit(Op::rgb_operation, stage.rgb_operation, i);
    submit(Op::rgb_argument_1, stage.rgb_argument_1, i);
    submit(Op::rgb_argument_2, stage.rgb_argument_2, i);
    submit(Op::alpha_operation, stage.alpha_operation, i);
    submit(Op::alpha_argument_1, stage.alpha_argument_1, i);
    submit(Op::alpha_argument_2, stage.alpha_argument_2, i);
  }
  submit(Op::null_stream_zero, c.strides[0]);
  submit(Op::null_indices, c.index_base_vertex);
  submit(Op::vertex_format, c.vertex_format);
  submit(Op::null_pixel_shader);
  c.material_cache = {0xffffffffU, 0, 0};
  c.projection_cache.fill(0);
  c.world_cache.fill(0);
  c.view_cache.fill(0);

  c.effective_features = static_cast<std::uint8_t>(~(c.disable_mask_a | c.disable_mask_b) & 3U);
  const auto captured_features = static_cast<std::uint8_t>(~(c.disable_mask_a | c.disable_mask_b) & 3U);
  const auto diagnostic = hooks.diagnostic_service();
  if (!diagnostic) throw std::runtime_error("picture reset diagnostic service is missing");
  diagnostic(captured_features);
  if (captured_features == 0) {
    const auto set = [&](auto& tracked, auto value, Op operation) {
      if (tracked != value) { tracked = value; submit(operation, value); }
    };
    auto& stage = c.stages[0];
    set(stage.rgb_operation, PictureStageOperation::select_argument_1, Op::rgb_operation);
    set(stage.rgb_argument_1, PictureStageArgument::texture_factor, Op::rgb_argument_1);
    set(stage.rgb_argument_2, PictureStageArgument::current, Op::rgb_argument_2);
    set(stage.alpha_operation, PictureStageOperation::select_argument_1, Op::alpha_operation);
    set(stage.alpha_argument_1, PictureStageArgument::texture_factor, Op::alpha_argument_1);
    set(stage.alpha_argument_2, PictureStageArgument::current, Op::alpha_argument_2);
    submit(Op::wireframe_fill);
  }
  if (c.lighting_enabled) {
    c.lighting_enabled = false;
    submit(Op::lighting_enabled, false);
  }
}
} // namespace off::graphics
