#pragma once

#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/intro_prepared_resources.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace off::graphics {

class IntroRuntime;

// Renderer-neutral destination extent.  The builder retains it as caller
// intent; projection, viewport policy, and GPU admission remain external.
struct IntroPreviewTarget {
  std::uint32_t width{};
  std::uint32_t height{};
};

// Only exact retained authored-picture routes are supported.  The stricter
// first-cut form requires the ordinary reader bracket's owner and component
// receipts to identify this same source.  Neither form has a generic
// source-class fallback or a synthesized image policy.
enum class IntroPreviewPolicy : std::uint8_t {
  exact_source_picture,
  admitted_first_cut_legal_picture,
};

struct IntroPreviewDraw {
  std::size_t source_index{};
  data::PictureDrawPlan draw_plan;
};

struct IntroPreviewSnapshot {
  IntroPreviewTarget target;
  IntroPreviewDraw draw;
  // Own pixels so a later renderer upload does not borrow IntroRuntime state.
  std::vector<IntroPreparedImage> images;
};

// Snapshots one already-retained intro picture and exactly the images named by
// its current draw plan.  It intentionally does not select a camera, derive a
// transform, submit a draw, or make the cut active.
//
// Unknown/non-picture source rows, absent required first-cut reader receipts,
// zero destination dimensions, unsupported policies, empty plans, duplicate
// image identities, and missing/invalid referenced images throw
// std::runtime_error before returning a snapshot.
[[nodiscard]] IntroPreviewSnapshot build_intro_preview(
    const IntroRuntime &runtime, std::size_t source_index,
    IntroPreviewTarget target,
    IntroPreviewPolicy policy = IntroPreviewPolicy::exact_source_picture);

} // namespace off::graphics
