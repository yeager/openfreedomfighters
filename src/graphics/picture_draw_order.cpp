#include "off/graphics/picture_draw_order.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>

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

void PictureRecordRebuild::accept(const PictureQueuedDrawRecord& record,
                                  const PictureRecordRebuildHooks& hooks) {
  if (accepting_ || rebuilding_ || poisoned_)
    throw std::runtime_error("picture record route is active or poisoned");
  if (!hooks.prepare_record || !hooks.register_record || !hooks.current_view)
    throw std::runtime_error("picture record route requires complete hooks");
  if (record.record_identity == 0 || record.owner_context_identity == 0 ||
      record.runtime_resource == 0)
    throw std::runtime_error("picture record route requires live record, owner and resource identities");
  // Validate the supported static resource marker before external effects.
  (void)make_picture_order_key(0, record.submission_control,
                               record.selected_resource);
  struct Guard {
    bool& active;
    explicit Guard(bool& value) : active(value) { active = true; }
    ~Guard() { active = false; }
  } guard(accepting_);
  try {
    hooks.prepare_record(record);
    hooks.register_record(record);
    queued_.push_back(record);
  } catch (...) {
    poisoned_ = true;
    throw;
  }
}

std::vector<PictureOrderedDrawEntry> PictureRecordRebuild::rebuild(
    std::span<const PictureOrderedDrawEntry> retained,
    const PictureRecordRebuildHooks& hooks) {
  if (accepting_ || rebuilding_ || poisoned_)
    throw std::runtime_error("picture record route is active or poisoned");
  if (!hooks.current_view)
    throw std::runtime_error("picture record rebuild requires a current-view service");
  struct Guard {
    bool& active;
    explicit Guard(bool& value) : active(value) { active = true; }
    ~Guard() { active = false; }
  } guard(rebuilding_);

  std::vector<PictureKeyedRecord> old_keys;
  std::vector<PictureKeyedRecord> new_keys;
  old_keys.reserve(retained.size());
  new_keys.reserve(queued_.size());
  std::unordered_map<std::uint64_t, PictureOrderedDrawEntry> entries;
  entries.reserve(retained.size() + queued_.size());
  const auto insert = [&](std::uint64_t identity, PictureOrderedDrawEntry entry) {
    if (identity == 0 || !entries.emplace(identity, entry).second)
      throw std::runtime_error("picture record route requires unique live record identities");
  };
  for (const auto& entry : retained) {
    insert(entry.record_identity, entry);
    old_keys.push_back({entry.record_identity, entry.key});
  }
  for (const auto& record : queued_) {
    const auto view = hooks.current_view(record);
    const auto key = make_picture_order_key(view ? view->order : 0,
                                            record.submission_control,
                                            record.selected_resource);
    PictureOrderedDrawEntry entry{key, record.record_identity,
                                  view ? std::optional{view->identity}
                                       : std::nullopt,
                                  record.runtime_resource};
    insert(record.record_identity, entry);
    new_keys.push_back({record.record_identity, key});
  }
  const auto ordered = merge_picture_draw_order(old_keys, new_keys);
  std::vector<PictureOrderedDrawEntry> result;
  result.reserve(ordered.size());
  for (const auto& ordered_entry : ordered) {
    auto entry = entries.at(ordered_entry.identity);
    entry.key = ordered_entry.key;
    result.push_back(entry);
  }
  queued_.clear();
  return result;
}

} // namespace off::graphics
