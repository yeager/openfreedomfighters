#include "off/graphics/picture_material_state.hpp"

#include <array>
#include <iostream>
#include <limits>

namespace {
using namespace off::graphics;
int failures{};
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
bool empty(const PictureStageRequests &stage) {
  return !stage.rgb_operation && !stage.rgb_argument_1 &&
         !stage.rgb_argument_2 && !stage.alpha_operation &&
         !stage.alpha_argument_1 && !stage.alpha_argument_2;
}
bool empty(const PictureMaterialRequests &state) {
  return !state.blend_enabled && !state.source_blend &&
         !state.destination_blend && !state.alpha_test_enabled &&
         !state.depth_write_enabled && !state.depth_comparison &&
         !state.cull_mode && !state.address_u && !state.address_v &&
         !state.texture_factor && !state.fog_color && empty(state.stage_zero);
}
PictureMaterialStateInput input(std::uint32_t word = 0, unsigned features = 3) {
  return {word, static_cast<std::uint32_t>(~features), 0, 0, std::nullopt, 0,
          true};
}
// Explicit independent renderer context for the pre-existing request tests;
// production callers must supply their live context, not use this fixture.
PictureMaterialStateRequests resolve_picture_material_state(const PictureMaterialStateInput& request) {
  return off::graphics::resolve_picture_material_state(request,
      {0x12345678U, 0x87654321U, 0xabcdefffU, 0x12345678U});
}
} // namespace

int main() {
  {
    const PictureRendererFogState fog{0x12345678U, 0x87654321U, 0xabcdefffU, 0};
    for (unsigned features = 0; features < 4; ++features) {
      for (const std::uint32_t word : {0U, 1U, 2U, 0x400U, 0x402U, 0x2000U, 0x2002U, 0xffffffffU}) {
        auto request = input(word, features);
        const auto result = off::graphics::resolve_picture_material_state(request, fog);
        const auto expected = (word & 2U) ? fog.additive_color :
                              (word & 0x2000U) ? fog.special_color : fog.base_color;
        check(features == 0 ? !result.material.fog_color : result.material.fog_color == expected,
              "fog uses additive before special before base on nonzero-feature material miss");
        auto matching = fog; matching.tracked_color = expected;
        check(!off::graphics::resolve_picture_material_state(request, matching).material.fog_color,
              "matching tracked color omits fog request without clearing backend state");
        request.cached_key = PictureMaterialCacheKey{word, 0xffffffffU, 0};
        check(!off::graphics::resolve_picture_material_state(request, fog).material.fog_color,
              "cache hit skips fog even when renderer colors have changed");
        request.cached_key.reset();
        for (const std::uint8_t suppression : {std::uint8_t{1}, std::uint8_t{255}}) {
          request.suppression_byte = suppression;
          const auto suppressed = off::graphics::resolve_picture_material_state(request, fog);
          check(!suppressed.material.fog_color && !suppressed.cache_replacement,
                "suppression skips fog and material cache writes");
          check(suppressed.resource_binding.bind_selected_texture.has_value() == ((features & 1U) != 0),
                "fog suppression does not suppress preceding admitted texture binding");
        }
      }
    }
    auto request = input(0x2000U);
    const auto special = off::graphics::resolve_picture_material_state(request, fog);
    check(special.cache_replacement == PictureMaterialCacheKey{0xffffffffU, 0xffffffffU, 0} &&
          special.material.fog_color == fog.special_color,
          "special cache-word replacement does not bypass fog request");
    const PictureRendererFogState zero_base{0, 7, 8, 1};
    check(off::graphics::resolve_picture_material_state(input(0), zero_base).material.fog_color == 0U,
          "packed zero is a real fog-color request, not absence");
    auto tracked = fog;
    // Apply only this documented tracked-color effect in a sequential fixture.
    // No backend initialization, full pass reset or draw admission is implied.
    for (const std::uint32_t word : {2U, 2U, 0x2000U, 0U}) {
      const auto next = off::graphics::resolve_picture_material_state(input(word), tracked);
      if (next.material.fog_color) tracked.tracked_color = *next.material.fog_color;
      check(tracked.tracked_color == ((word & 2U) ? fog.additive_color :
            (word & 0x2000U) ? fog.special_color : fog.base_color),
            "ordered material calls retain and replace explicit tracked fog");
    }
    check(tracked.base_color == fog.base_color && tracked.additive_color == fog.additive_color &&
          tracked.special_color == fog.special_color, "material selection never rewrites configured fog colors");
  }
  constexpr std::array<std::uint32_t, 7> mapped{
      0x60010, 0x60012, 0x60014, 0x60011, 0x60018, 0x60210, 0x60211};
  for (std::uint32_t i = 0; i < mapped.size(); ++i) {
    check(map_base_picture_material_property(i) == mapped[i],
          "base property mapping table");
    check(map_base_picture_material_property(i, true) == (mapped[i] | 1U),
          "optional setter override is OR, including already-set bit");
  }
  for (std::uint32_t value : {7U, 8U, 0x60010U, 0xffffffffU}) {
    check(map_base_picture_material_property(value) == value,
          "expanded properties pass through unsigned");
    check(map_base_picture_material_property(value, true) == (value | 1U),
          "override on expanded property");
  }
  for (unsigned features = 0; features < 4; ++features) {
    auto request = input(0, features);
    // Both masks participate, and unrelated high bits do not create features.
    request.pass_disable_mask_b = request.pass_disable_mask_a;
    const auto result = resolve_picture_material_state(request);
    check(result.effective_features == features, "effective feature mask");
    check(result.cache_replacement ==
              PictureMaterialCacheKey{0, 0xffffffffU, 0},
          "replace exact picture cache triple");
    if ((features & 1U) == 0) {
      check(!result.resource_binding.bind_selected_texture &&
                empty(result.resource_binding.stage_zero),
            "texture-disabled resource request omitted entirely");
    } else {
      const auto &stage = result.resource_binding.stage_zero;
      check(result.resource_binding.bind_selected_texture == true &&
                stage.rgb_argument_1 == PictureStageArgument::texture &&
                stage.alpha_argument_1 == PictureStageArgument::texture,
            "resource texture and first arguments");
      const auto operation = features == 3
                                 ? PictureStageOperation::modulate_twice
                                 : PictureStageOperation::select_argument_1;
      check(stage.rgb_operation == operation &&
                stage.alpha_operation == operation,
            "RGB and alpha have matching conditional operation");
      check(stage.rgb_argument_2.has_value() == (features == 3) &&
                stage.alpha_argument_2.has_value() == (features == 3),
            "second arguments omitted for texture-only selection");
      if (features == 3)
        check(stage.rgb_argument_2 == PictureStageArgument::diffuse &&
                  stage.alpha_argument_2 == PictureStageArgument::diffuse,
              "double modulation uses diffuse for RGB and alpha");
    }
    check(features == 0 ? empty(result.material)
                        : result.material.blend_enabled == false,
          "feature two still enters material branch");
    request.resource_transition = false;
    const auto repeated = resolve_picture_material_state(request);
    check(!repeated.resource_binding.bind_selected_texture &&
              empty(repeated.resource_binding.stage_zero) &&
              repeated.cache_replacement.has_value(),
          "no transition skips binding, not material request");
  }
  auto mixed = input();
  mixed.pass_disable_mask_a = 1;
  mixed.pass_disable_mask_b = 2;
  check(resolve_picture_material_state(mixed).effective_features == 0,
        "union both pass disable masks");
  for (std::uint8_t suppress : {std::uint8_t{1}, std::uint8_t{255}}) {
    auto request = input(0x2000);
    request.suppression_byte = suppress;
    request.cached_key = PictureMaterialCacheKey{42, 1, 9};
    const auto result = resolve_picture_material_state(request);
    check(empty(result.material) && !result.cache_replacement &&
              result.resource_binding.bind_selected_texture == true,
          "suppression leaves cache and material untouched but preserves prior "
          "binding");
  }
  auto cached = input(0x2000, 2);
  cached.cached_key = PictureMaterialCacheKey{0x2000, 0xffffffffU, 0};
  check(empty(resolve_picture_material_state(cached).material) &&
            !resolve_picture_material_state(cached).cache_replacement,
        "cache hit does not include effective features");
  cached.pass_disable_mask_a = 0;
  const auto rebound = resolve_picture_material_state(cached);
  check(empty(rebound.material) &&
            rebound.resource_binding.stage_zero.rgb_operation ==
                PictureStageOperation::modulate_twice,
        "changed features retain cache hit but binding still occurs first");
  for (unsigned field = 0; field < 3; ++field) {
    auto mismatch = cached;
    if (field == 0)
      mismatch.cached_key->material_word ^= 1;
    if (field == 1)
      mismatch.cached_key->secondary_word ^= 1;
    if (field == 2)
      mismatch.cached_key->alpha_threshold = 1;
    check(
        resolve_picture_material_state(mismatch).cache_replacement.has_value(),
        "every cache triple component participates");
  }
  for (std::uint32_t mode : {0U, 1U, 2U, 0xffffffffU}) {
    auto request = input(0x2000, 0);
    request.mode_selector = mode;
    auto result = resolve_picture_material_state(request);
    check(result.cache_replacement.has_value() &&
              !result.material.blend_enabled &&
              result.material.texture_factor.has_value() == (mode == 1),
          "zero features records cache before optional texture factor request");
    if (mode == 1)
      check(result.material.texture_factor == 0xffffffffU,
            "picture secondary is unsigned all ones");
    check(result.cache_replacement->material_word == 0x2000U,
          "zero-feature special bit does not replace cache word with sentinel");
    request.cached_key = result.cache_replacement;
    check(empty(resolve_picture_material_state(request).material),
          "zero-feature cache hit suppresses texture factor too");
  }
  for (unsigned bit = 0; bit < 32; ++bit) {
    const auto word = std::uint32_t{1} << bit;
    const auto result = resolve_picture_material_state(input(word));
    check(result.material.blend_enabled == ((word & 0x402607U) != 0),
          "every material bit tested against blend-enable mask");
    check(result.material.alpha_test_enabled == false &&
              !result.material.texture_factor,
          "normal picture branch disables alpha test and omits texture factor");
    if ((word & 0x402607U) == 0)
      check(!result.material.source_blend && !result.material.destination_blend,
            "disabled blend omits factors");
  }
  for (auto word : {1U, 2U, 3U, 4U, 0x200U, 0x400U, 0x402U, 0x400000U}) {
    const auto state = resolve_picture_material_state(input(word)).material;
    check(state.source_blend == PictureBlendFactor::source_alpha &&
              state.destination_blend ==
                  (((word & 2U) && !(word & 0x400U))
                       ? PictureBlendFactor::one
                       : PictureBlendFactor::inverse_source_alpha) &&
              empty(state.stage_zero),
          "ordinary blend factors do not rewrite texture operations");
  }
  for (unsigned features : {1U, 2U, 3U}) {
    const auto result = resolve_picture_material_state(input(0x2000, features));
    const auto &state = result.material;
    check(result.cache_replacement ==
              PictureMaterialCacheKey{0xffffffffU, 0xffffffffU, 0},
          "active special branch stores ordinary all-ones cache material word");
    auto next = input(0xffffffffU, features);
    next.cached_key = result.cache_replacement;
    check(empty(resolve_picture_material_state(next).material) &&
              !resolve_picture_material_state(next).cache_replacement,
          "all-ones material input can exactly hit special branch cache word");
    next.runtime_material_word = 0x2000U;
    check(resolve_picture_material_state(next).material.blend_enabled == true,
          "repeated original special word misses replaced cache key");
    check(
        state.blend_enabled == true &&
            state.source_blend == PictureBlendFactor::zero &&
            state.destination_blend == PictureBlendFactor::source_color &&
            state.stage_zero.rgb_operation == PictureStageOperation::add &&
            state.stage_zero.rgb_argument_1 == PictureStageArgument::texture &&
            state.stage_zero.rgb_argument_2 == PictureStageArgument::diffuse &&
            state.stage_zero.alpha_operation ==
                PictureStageOperation::disable &&
            !state.stage_zero.alpha_argument_1 &&
            !state.stage_zero.alpha_argument_2,
        "special material overrides RGB and alpha even with texture feature "
        "disabled");
  }
  for (std::uint32_t flags = 0; flags < 32; ++flags) {
    const std::uint32_t word =
        ((flags & 1U) ? 0x40000U : 0U) | ((flags & 2U) ? 0x20000U : 0U) |
        ((flags & 4U) ? 0x80000U : 0U) | ((flags & 8U) ? 0x4000U : 0U) |
        ((flags & 16U) ? 0x8000U : 0U);
    const auto state = resolve_picture_material_state(input(word, 2)).material;
    check(state.depth_write_enabled == ((flags & 1U) == 0) &&
              state.depth_comparison ==
                  ((flags & 2U) ? PictureDepthComparison::always
                                : PictureDepthComparison::less_equal) &&
              state.cull_mode == ((flags & 4U) ? PictureCullMode::none
                                               : PictureCullMode::clockwise) &&
              state.address_u == ((flags & 8U) ? PictureAddressMode::clamp
                                               : PictureAddressMode::wrap) &&
              state.address_v == ((flags & 16U) ? PictureAddressMode::clamp
                                                : PictureAddressMode::wrap),
          "independent depth/culling/addressing masks with texture feature "
          "disabled");
  }
  for (const auto original : {0U, 1U, 0x60010U, 0x60011U, 0xfffffffeU,
                              0xffffffffU}) {
    for (const auto alpha : {0U, 254U, 255U, 256U, 511U, 0xffffffffU}) {
      const auto transition = update_picture_alpha_material(original, alpha);
      check((transition.material_word & ~1U) == (original & ~1U),
            "alpha transition preserves every unrelated material bit");
      check((transition.material_word & 1U) == (alpha == 255U ? 0U : 1U),
            "alpha transition compares the entire input, not its low byte");
      check(transition.material_changed ==
                (transition.material_word != original),
            "alpha transition reports exactly a material change");
      const auto repeated =
          update_picture_alpha_material(transition.material_word, alpha);
      check(!repeated.material_changed &&
                repeated.material_word == transition.material_word,
            "alpha transition is idempotent for repeated input");
    }
  }
  const auto opaque = map_base_picture_material_property(0);
  const auto faded = update_picture_alpha_material(opaque, 254);
  check(resolve_picture_material_state(input(faded.material_word))
            .material.blend_enabled == true &&
            resolve_picture_material_state(input(
                update_picture_alpha_material(faded.material_word, 255)
                    .material_word)).material.blend_enabled == false,
        "explicit alpha events change conditional blend requests without a fade default");
  return failures == 0 ? 0 : 1;
}
