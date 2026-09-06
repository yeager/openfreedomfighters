#include "off/graphics/picture_draw_order.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace off::graphics {

std::uint32_t make_picture_order_key(
    std::uint8_t view_order, std::uint8_t submission_control,
    const data::PictureTextureBinding& selected_resource) {
  constexpr std::array marker{std::byte{0}, std::byte{1}, std::byte{2}, std::byte{0}};
  if (!std::equal(marker.begin(), marker.end(), selected_resource.authored_texture_resource_record.begin()))
    throw std::runtime_error("picture order key requires the checked static picture marker");
  // Cast before shifting: overflow intentionally wraps in unsigned32, including
  // the fifth view bit. Full-key sorting must not mask off that high bit.
  return (static_cast<std::uint32_t>(view_order) << 27U) |
         (static_cast<std::uint32_t>(submission_control) << 19U) |
         (13U << 15U) |
         ((static_cast<std::uint32_t>(selected_resource.texture_id) & 0x7ffU) << 4U) |
         2U;
}

std::vector<PictureOrderedRecord> merge_picture_draw_order(
    std::span<const PictureKeyedRecord> retained,
    std::span<const PictureKeyedRecord> rebuilt) {
  const auto less = [](const auto& a, const auto& b) { return a.key < b.key; };
  if (!std::is_sorted(retained.begin(), retained.end(), less))
    throw std::runtime_error("retained picture records must be unsigned-key sorted");
  std::vector<PictureOrderedRecord> result;
  if (retained.size() > result.max_size() || rebuilt.size() > result.max_size() - retained.size())
    throw std::runtime_error("picture draw order exceeds native container capacity");
  std::vector<PictureKeyedRecord> sorted(rebuilt.begin(), rebuilt.end());
  std::stable_sort(sorted.begin(), sorted.end(), less);
  result.reserve(retained.size() + sorted.size());
  std::size_t old_index = 0, new_index = 0;
  while (old_index < retained.size() || new_index < sorted.size()) {
    const bool use_new = new_index < sorted.size() &&
        (old_index == retained.size() || sorted[new_index].key <= retained[old_index].key);
    const auto& next = use_new ? sorted[new_index++] : retained[old_index++];
    result.push_back({next.identity, next.key, result.size()});
  }
  return result;
}

} // namespace off::graphics
