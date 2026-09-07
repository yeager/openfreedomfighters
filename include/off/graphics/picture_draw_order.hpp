#pragma once

#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/picture_ordered_draw_loop.hpp"

#include <cstdint>
#include <functional>
#include <optional>
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

// A caller-owned, source-backed submission record that has already passed its
// real picture-group visitor. `runtime_resource` is a live renderer resource
// identity, not a texture catalogue index or a GPU handle. This model neither
// parses source data nor manufactures a record/resource when one is absent.
struct PictureQueuedDrawRecord {
  std::uint64_t record_identity;
  std::uint64_t owner_context_identity;
  std::uint64_t runtime_resource;
  data::PictureTextureBinding selected_resource;
  std::uint8_t submission_control;
};

struct PictureCurrentDrawView {
  std::uint64_t identity;
  std::uint8_t order;
};

struct PictureRecordRebuildHooks {
  // These remain concrete owner/backend services. Returning successfully does
  // not mean a texture was uploaded or a device draw was accepted.
  std::function<void(const PictureQueuedDrawRecord&)> prepare_record;
  std::function<void(const PictureQueuedDrawRecord&)> register_record;
  // Resolves the record's *current* owner-context view at rebuild time. Null
  // represents no associated view and produces view order zero.
  std::function<std::optional<PictureCurrentDrawView>(
      const PictureQueuedDrawRecord&)> current_view;
};

// Bounded bridge from actual accepted picture records to the existing ordered
// draw loop. It deliberately stops at `PictureOrderedDrawEntry`: caller-owned
// ordered dispatch still supplies view transition, resource binding and emit;
// no texture upload, GPU submission, device success or presentation is implied.
class PictureRecordRebuild final {
public:
  PictureRecordRebuild() = default;
  PictureRecordRebuild(const PictureRecordRebuild&) = delete;
  PictureRecordRebuild& operator=(const PictureRecordRebuild&) = delete;
  PictureRecordRebuild(PictureRecordRebuild&&) = delete;
  PictureRecordRebuild& operator=(PictureRecordRebuild&&) = delete;

  [[nodiscard]] bool poisoned() const noexcept { return poisoned_; }
  [[nodiscard]] std::size_t queued_count() const noexcept { return queued_.size(); }

  // Preparation and registration precede queue publication. Invalid input,
  // missing hooks, reentry and an already-poisoned route reject before effects.
  // A callback failure poisons the route and retains only the earlier queued
  // prefix; it never invents rollback or registration success.
  void accept(const PictureQueuedDrawRecord& record,
              const PictureRecordRebuildHooks& hooks);

  // Retained keys must already be unsigned sorted. Queue records are keyed
  // from their live view at this call, then stably merged using the existing
  // recovered new-before-retained equal-key policy. Record identities must be
  // unique across the supplied retained and queued collections: this is a
  // native safety bound for unambiguous live association, not source admission.
  // The pending queue clears only after the complete result is formed. Missing
  // live views use order zero; no lookup creates a camera/view. Rebuild needs
  // only the current-view service; preparation/registration occur in accept.
  [[nodiscard]] std::vector<PictureOrderedDrawEntry> rebuild(
      std::span<const PictureOrderedDrawEntry> retained,
      const PictureRecordRebuildHooks& hooks);

private:
  bool accepting_{false};
  bool rebuilding_{false};
  bool poisoned_{false};
  std::vector<PictureQueuedDrawRecord> queued_;
};

} // namespace off::graphics
