#pragma once

#include "off/data/picture_texture_binding.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace off::graphics {

// Accepted static-selection picture record with the checked source marker.
// View order and submission control are current runtime bytes, not source
// directory indices. The selected TEX image ID, not catalog position, supplies
// the binding field. Source-marker mutation/animated selection is unsupported.
[[nodiscard]] std::uint32_t make_picture_order_key(
    std::uint8_t view_order, std::uint8_t submission_control,
    const data::PictureTextureBinding& selected_resource);

struct PictureKeyedRecord {
  std::uint64_t identity;
  std::uint32_t key;
};

struct PictureOrderedRecord {
  std::uint64_t identity;
  std::uint32_t key;
  std::size_t slot_index;
};

// Caller supplies already-compacted active retained entries and eligible
// rebuilt entries; this does not infer registration/visibility/queue flags.
// Retained keys must already be unsigned ascending. No entry is deduplicated.
// NEW equal-key entries use stable input order as explicit native policy:
// the original comparator violated the sort equality contract. New equal keys
// precede retained equal keys, preserving the recovered cross-partition rule.
// Returns every resulting slot without mutating caller records on failure.
[[nodiscard]] std::vector<PictureOrderedRecord> merge_picture_draw_order(
    std::span<const PictureKeyedRecord> retained,
    std::span<const PictureKeyedRecord> rebuilt);

} // namespace off::graphics
