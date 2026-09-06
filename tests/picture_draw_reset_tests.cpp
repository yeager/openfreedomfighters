#include "off/graphics/picture_draw_reset.hpp"
#include "off/graphics/picture_ordered_draw_loop.hpp"

#include <algorithm>
#include <bit>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace off::graphics;
using Op = PictureResetOperation;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
PictureDrawContext fixture() {
  PictureDrawContext c{};
  c.fog = {{11, 12, 13, 14}, true, false, 15, 16};
  c.material_cache = {17, 18, 19};
  c.projection_cache.fill(21); c.world_cache.fill(22); c.view_cache.fill(23);
  for (auto& s : c.stages)
    s = {77, PictureStageOperation::add, PictureStageArgument::diffuse, PictureStageArgument::texture,
         PictureStageOperation::modulate_twice, PictureStageArgument::diffuse, PictureStageArgument::texture};
  c.source_blend = PictureBlendFactor::source_alpha;
  c.destination_blend = PictureBlendFactor::source_color;
  c.blend_enabled = true; c.depth_write_enabled = false; c.depth_write_suppressed = true;
  c.depth_comparison = PictureDepthComparison::always;
  c.alpha_comparison = PictureDepthComparison::less_equal;
  c.alpha_test_enabled = true; c.lighting_enabled = true; c.cull_mode = PictureCullMode::none;
  c.streams = {31, 32}; c.strides = {33, 34}; c.indices = 35;
  c.index_base_vertex = 0xdeadbeefU; c.vertex_format = 36; c.pixel_shader = 37;
  c.disable_mask_a = 0x10; c.disable_mask_b = 0x20; c.material_mode = 39;
  c.material_suppression = 40; c.effective_features = 41;
  return c;
}
bool old_caches(const PictureDrawContext& c) {
  return c.material_cache == PictureMaterialCacheKey{17, 18, 19} &&
      std::all_of(c.projection_cache.begin(), c.projection_cache.end(), [](float x) { return x == 21; }) &&
      std::all_of(c.world_cache.begin(), c.world_cache.end(), [](float x) { return x == 22; }) &&
      std::all_of(c.view_cache.begin(), c.view_cache.end(), [](float x) { return x == 23; });
}
bool reset_caches(const PictureDrawContext& c) {
  const auto zero = [](float x) { return std::bit_cast<std::uint32_t>(x) == 0; };
  return c.material_cache == PictureMaterialCacheKey{0xffffffffU, 0, 0} &&
      std::all_of(c.projection_cache.begin(), c.projection_cache.end(), zero) &&
      std::all_of(c.world_cache.begin(), c.world_cache.end(), zero) &&
      std::all_of(c.view_cache.begin(), c.view_cache.end(), zero);
}
bool reset_stage(const PictureTrackedStage& s) {
  return !s.texture && s.rgb_operation == PictureStageOperation::disable &&
      s.rgb_argument_1 == PictureStageArgument::texture && s.rgb_argument_2 == PictureStageArgument::current &&
      s.alpha_operation == PictureStageOperation::disable && s.alpha_argument_1 == PictureStageArgument::texture &&
      s.alpha_argument_2 == PictureStageArgument::current;
}
bool tracked_reset(const PictureDrawContext& c) {
  return c.source_blend == PictureBlendFactor::one && c.destination_blend == PictureBlendFactor::zero &&
      !c.blend_enabled && c.depth_write_enabled && !c.depth_write_suppressed &&
      c.depth_comparison == PictureDepthComparison::less_equal && !c.alpha_test_enabled &&
      c.alpha_comparison == PictureDepthComparison::always && !c.lighting_enabled &&
      c.cull_mode == PictureCullMode::clockwise && c.fog.tracked_enabled && !c.fog.suppression_latched &&
      c.fog.colors.tracked_color == 0 && c.fog.colors.base_color == 0 &&
      c.fog.colors.additive_color == 0xff000000U && c.fog.colors.special_color == 0xffffffffU &&
      std::bit_cast<std::uint32_t>(c.fog.start) == 0x3f333333U &&
      std::bit_cast<std::uint32_t>(c.fog.end) == 0x3f800000U &&
      !c.streams[0] && !c.streams[1] && c.strides == std::array<std::uint32_t, 2>{0, 0} &&
      !c.indices && c.vertex_format == 0x142U && !c.pixel_shader;
}
std::vector<PictureResetCommand> expected(std::uint32_t stages) {
  std::vector<PictureResetCommand> result;
  const auto add = [&](Op op, PictureResetValue value = {}, std::uint32_t stage = 0) {
    result.push_back({op, stage, value});
  };
  add(Op::source_blend, PictureBlendFactor::one); add(Op::destination_blend, PictureBlendFactor::zero);
  add(Op::blend_enabled, false); add(Op::depth_write_enabled, true);
  add(Op::depth_comparison, PictureDepthComparison::less_equal); add(Op::alpha_comparison, PictureDepthComparison::always);
  add(Op::alpha_test_enabled, false); add(Op::fog_enabled, true); add(Op::fog_color, std::uint32_t{0});
  add(Op::fog_start, std::bit_cast<float>(0x3f333333U)); add(Op::fog_end, std::bit_cast<float>(0x3f800000U));
  add(Op::lighting_enabled, false); add(Op::cull_mode, PictureCullMode::clockwise);
  for (std::uint32_t i = 0; i < stages; ++i) {
    add(Op::null_texture, {}, i); add(Op::rgb_operation, PictureStageOperation::disable, i);
    add(Op::rgb_argument_1, PictureStageArgument::texture, i); add(Op::rgb_argument_2, PictureStageArgument::current, i);
    add(Op::alpha_operation, PictureStageOperation::disable, i);
    add(Op::alpha_argument_1, PictureStageArgument::texture, i); add(Op::alpha_argument_2, PictureStageArgument::current, i);
  }
  add(Op::null_stream_zero, std::uint32_t{0}); add(Op::null_indices, std::uint32_t{0xdeadbeefU});
  add(Op::vertex_format, std::uint32_t{0x142U}); add(Op::null_pixel_shader);
  return result;
}
bool same(const PictureResetCommand& a, const PictureResetCommand& b) {
  return a.operation == b.operation && a.stage == b.stage && a.value == b.value;
}
PictureDrawResetHooks quiet_hooks() { return {[](const auto&) {}, [] { return [](std::uint8_t) {}; }}; }
}

int main() {
  static_assert(!std::is_copy_constructible_v<PictureDrawReset>);
  static_assert(!std::is_move_constructible_v<PictureDrawReset>);
  for (const std::uint32_t count : {0U, 1U, 8U}) {
    auto c = fixture(); PictureDrawReset reset;
    std::vector<PictureResetCommand> commands; std::vector<std::string> boundaries;
    const auto baseline = expected(count);
    reset.run(c, count, {[&](const auto& command) {
      check(tracked_reset(c) && old_caches(c) && c.effective_features == 41,
            "every unconditional backend callback sees completed tracked reset but old material/matrix caches");
      for (std::size_t i = 0; i < c.stages.size(); ++i)
        check(i < count ? reset_stage(c.stages[i]) : c.stages[i].texture == 77,
              "tracked stages reset only within explicit effective count");
      commands.push_back(command);
    }, [&] {
      boundaries.push_back("getter");
      check(reset_caches(c) && c.effective_features == 3 && commands.size() == baseline.size(),
            "diagnostic getter follows entire backend application and material/matrix clears");
      return [&](std::uint8_t features) { boundaries.push_back("format"); check(features == 3, "diagnostic captures feature bits"); };
    }});
    check(commands.size() == 13 + 7 * count + 4 && commands.size() == baseline.size() &&
          std::equal(commands.begin(), commands.end(), baseline.begin(), same),
          "unconditional command order and typed payloads exactly match bounded reset");
    check(boundaries == std::vector<std::string>{"getter", "format"}, "diagnostic getter precedes formatter");
    check(c.index_base_vertex == 0xdeadbeefU && c.disable_mask_a == 0x10 && c.disable_mask_b == 0x20 &&
          c.material_mode == 39 && c.material_suppression == 40,
          "index base and material feature/mode/suppression inputs survive reset");
    std::size_t repeated = 0;
    reset.run(c, count, {[&](const auto&) { ++repeated; }, [] { return [](std::uint8_t) {}; }});
    check(repeated == baseline.size(), "equal tracked values never suppress unconditional reset commands");
  }
  {
    auto c = fixture(); c.disable_mask_a = 1; c.disable_mask_b = 2;
    PictureDrawReset reset; std::vector<PictureResetCommand> commands;
    reset.run(c, 1, {[&](const auto& cmd) { commands.push_back(cmd); }, [] { return [](std::uint8_t) {}; }});
    const auto base = expected(1).size();
    const std::array<Op, 5> tail{Op::rgb_operation, Op::rgb_argument_1, Op::alpha_operation, Op::alpha_argument_1, Op::wireframe_fill};
    check(commands.size() == base + tail.size(), "untouched zero-feature fallback skips both redundant CURRENT arguments");
    for (std::size_t i = 0; i < tail.size() && base + i < commands.size(); ++i)
      check(commands[base + i].operation == tail[i] && commands[base + i].stage == 0,
            "RGB fallback precedes alpha fallback and unconditional wireframe");
    check(c.stages[0].rgb_argument_1 == PictureStageArgument::texture_factor &&
          c.stages[0].alpha_argument_1 == PictureStageArgument::texture_factor,
          "fallback uses texture factor rather than diffuse color");
  }
  {
    auto c = fixture(); c.disable_mask_a = 3;
    PictureDrawReset reset; std::vector<PictureResetCommand> commands;
    reset.run(c, 0, {[&](const auto& cmd) { commands.push_back(cmd); }, [&] {
      check(c.effective_features == 0 && reset_caches(c), "getter sees stored pre-callback features and cleared caches");
      c.disable_mask_a = 0; c.disable_mask_b = 0;
      return [&](std::uint8_t captured) {
        check(captured == 0, "mask mutations in getter cannot change captured diagnostic bits");
        c.effective_features = 2;
        auto& stage = c.stages[0];
        stage.rgb_operation = PictureStageOperation::select_argument_1;
        stage.rgb_argument_1 = PictureStageArgument::texture_factor;
        stage.rgb_argument_2 = PictureStageArgument::diffuse;
        stage.alpha_operation = PictureStageOperation::select_argument_1;
        stage.alpha_argument_1 = PictureStageArgument::texture_factor;
        stage.alpha_argument_2 = PictureStageArgument::diffuse;
        c.lighting_enabled = true;
      };
    }});
    const auto base = expected(0).size();
    check(commands.size() == base + 4 && commands[base].operation == Op::rgb_argument_2 &&
          commands[base + 1].operation == Op::alpha_argument_2 && commands[base + 2].operation == Op::wireframe_fill &&
          commands[base + 3].operation == Op::lighting_enabled,
          "captured zero branch uses post-callback tracked stage differences and current lighting");
    check(c.effective_features == 2 && !c.lighting_enabled,
          "fallback does not overwrite diagnostic effective-feature mutation");
  }
  {
    auto c = fixture(); PictureDrawReset reset; std::vector<PictureResetCommand> commands;
    reset.run(c, 1, {[&](const auto& cmd) { commands.push_back(cmd); }, [&] {
      c.disable_mask_a = 3;
      return [&](std::uint8_t captured) { check(captured == 3, "nonzero captured bits survive changed masks"); c.lighting_enabled = true; };
    }});
    check(commands.size() == expected(1).size() + 1 && commands.back().operation == Op::lighting_enabled,
          "captured nonzero branch skips wireframe despite post-callback zero masks but corrects lighting");
  }
  {
    for (int invalid = 0; invalid < 3; ++invalid) {
      auto c = fixture(); PictureDrawReset reset; unsigned calls = 0; auto hooks = quiet_hooks();
      hooks.submit = [&](const auto&) { ++calls; };
      if (invalid == 0) hooks.submit = {};
      if (invalid == 1) hooks.diagnostic_service = {};
      rejects([&] { reset.run(c, invalid == 2 ? 9 : 1, hooks); }, "invalid count or missing hooks rejects");
      check(calls == 0 && old_caches(c) && c.streams[1] == 32 && c.effective_features == 41,
            "prevalidation errors have no tracked or backend effects");
    }
    auto c = fixture(); PictureDrawReset reset; unsigned calls = 0;
    rejects([&] { reset.run(c, 1, {[&](const auto&) { ++calls; }, [] { return std::function<void(std::uint8_t)>{}; }}); },
            "missing returned diagnostic service rejects after reset boundary");
    check(calls == expected(1).size() && reset_caches(c) && c.effective_features == 3,
          "late missing diagnostic retains completed reset/cache prefix");
  }
  {
    const auto baseline = expected(1);
    for (std::size_t fail = 1; fail <= baseline.size(); ++fail) {
      auto c = fixture(); PictureDrawReset reset; std::size_t calls = 0; bool diagnostic = false;
      rejects([&] { reset.run(c, 1, {[&](const auto&) {
        if (++calls == fail) throw std::runtime_error("backend reset failure");
      }, [&] { diagnostic = true; return [](std::uint8_t) {}; }}); }, "backend reset failure propagates");
      check(calls == fail && !diagnostic && tracked_reset(c) && old_caches(c) && c.effective_features == 41,
            "backend failure retains tracked reset and old caches without entering diagnostic");
      reset.run(c, 1, quiet_hooks());
      check(reset_caches(c), "failure releases reentry guard for separately admitted later reset");
    }
    for (bool getter : {true, false}) {
      auto c = fixture(); PictureDrawReset reset;
      rejects([&] { reset.run(c, 1, {[](const auto&) {}, [getter]() -> std::function<void(std::uint8_t)> {
        if (getter) throw std::runtime_error("getter failure");
        return [](std::uint8_t) { throw std::runtime_error("formatter failure"); };
      }}); }, "diagnostic boundary exceptions propagate");
      check(reset_caches(c) && c.effective_features == 3, "diagnostic failure retains reset material/matrix caches");
    }
  }
  {
    auto c = fixture(); PictureDrawReset reset; unsigned callbacks = 0;
    reset.run(c, 1, {[&](const auto&) {
      ++callbacks;
      rejects([&] { reset.run(c, 1, quiet_hooks()); }, "backend sink cannot reenter reset");
    }, [&] {
      rejects([&] { reset.run(c, 1, quiet_hooks()); }, "diagnostic getter cannot reenter reset");
      return [&](std::uint8_t) { rejects([&] { reset.run(c, 1, quiet_hooks()); }, "diagnostic formatter cannot reenter reset"); };
    }});
    check(callbacks == expected(1).size() && reset_caches(c), "caught reentry does not disrupt outer reset");
  }
  {
    auto c = fixture(); c.material_suppression = 0;
    PictureDrawReset reset; PictureOrderedDrawLoop loop; std::size_t cursor = 0; unsigned resets = 0;
    const std::array<PictureOrderedDrawEntry, 2> entries{{{9, 1, {}, {}}, {0x08000012U, 2, 101, 201}}};
    PictureOrderedDrawHooks hooks{
      [&] { ++resets; reset.run(c, 1, quiet_hooks()); }, [](auto) {}, [](auto) {}, [](auto) {}, [](auto) {},
      [&](auto, auto) {
        check(c.material_cache == PictureMaterialCacheKey{0xffffffffU, 0, 0}, "ordered emission sees ordinary reset cache");
        const PictureMaterialStateInput input{2, c.disable_mask_a, c.disable_mask_b, c.material_suppression,
                                             c.material_cache, c.material_mode, false};
        const auto result = resolve_picture_material_state(input, c.fog.colors);
        check(result.material.fog_color == 0xff000000U && result.cache_replacement.has_value(),
              "same canonical reset fog/cache feed actual material request planner");
        if (result.material.fog_color) c.fog.colors.tracked_color = *result.material.fog_color;
        if (result.cache_replacement) c.material_cache = *result.cache_replacement;
      }};
    check(loop.run(entries, cursor, hooks) && cursor == 1, "first barrier round performs reset");
    check(!loop.run(entries, cursor, hooks) && cursor == 2, "continuation emits after independent reset");
    check(!loop.run(entries, cursor, hooks) && resets == 3 && reset_caches(c) && c.fog.colors.tracked_color == 0,
          "exhausted ordered invocation still resets shared material/fog state");
  }
  return failures == 0 ? 0 : 1;
}
