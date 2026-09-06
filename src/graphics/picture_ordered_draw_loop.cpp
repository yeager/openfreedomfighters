#include "off/graphics/picture_ordered_draw_loop.hpp"

#include <limits>
#include <stdexcept>

namespace off::graphics {
bool PictureOrderedDrawLoop::run(std::span<const PictureOrderedDrawEntry> entries,
    std::size_t& prepared_cursor, const PictureOrderedDrawHooks& hooks) {
  constexpr std::uint32_t view_mask = 0x78000000U;
  constexpr std::uint32_t resource_mask = 0x7ff0U;
  constexpr auto sentinel = std::numeric_limits<std::uint32_t>::max();
  if (running_ || prepared_cursor > entries.size())
    throw std::runtime_error("invalid or reentrant ordered draw invocation");
  if (!hooks.reset || !hooks.view_transition || !hooks.subtype_begin ||
      !hooks.subtype_end || !hooks.bind_resource || !hooks.emit)
    throw std::runtime_error("ordered drawing requires complete hooks");
  for (const auto& entry : entries.subspan(prepared_cursor)) {
    if (entry.key == sentinel)
      throw std::runtime_error("ordered draw key collides with unsupported initial sentinel");
    if ((entry.key & 15U) != 9U && (entry.key & view_mask) != view_mask && !entry.associated_view)
      throw std::runtime_error("ordered draw record lacks its live view association");
  }
  struct Guard {
    bool& running;
    explicit Guard(bool& value) : running(value) { running = true; }
    ~Guard() { running = false; }
  } guard(running_);
  hooks.reset();
  const auto end = entries.size();
  auto cursor = prepared_cursor;
  if (cursor == end) return false;
  std::uint32_t previous_key = sentinel;
  std::optional<std::uint8_t> previous_subtype;
  while (cursor != end) {
    const auto& entry = entries[cursor];
    if (entry.key != previous_key) {
      const auto subtype = static_cast<std::uint8_t>(entry.key & 15U);
      if (subtype == 9U) { ++cursor; break; }
      if ((entry.key & view_mask) != (previous_key & view_mask)) {
        if ((entry.key & view_mask) == view_mask) { ++cursor; continue; }
        hooks.view_transition(*entry.associated_view);
      }
      if (subtype != previous_subtype) {
        if (previous_subtype) hooks.subtype_end(*previous_subtype);
        hooks.subtype_begin(subtype);
        previous_subtype = subtype;
      }
      if ((entry.key & resource_mask) != (previous_key & resource_mask) && entry.resource)
        hooks.bind_resource(*entry.resource);
      previous_key = entry.key;
    }
    hooks.emit(entry.record_identity, *previous_subtype);
    ++cursor;
  }
  prepared_cursor = cursor;
  if (previous_subtype) hooks.subtype_end(*previous_subtype);
  return cursor != end;
}
} // namespace off::graphics
