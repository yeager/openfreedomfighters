#pragma once

#include "off/graphics/picture_ordered_draw_loop.hpp"

#include <vector>

namespace off::graphics {

struct PictureOrderedState {
  std::uint64_t identity;
  std::span<PictureOrderedDrawEntry> entries;
  std::vector<std::uint64_t> selected_records;
  // Null means inactive, which is distinct from an exhausted active cursor.
  std::optional<std::size_t> cursor;
};

struct PictureOrderedCoordinatorHooks {
  using Select = std::function<void(std::uint64_t)>;
  // Populate only through the synchronous bounded visitor. Selection registry
  // queries/removals remain this required service's responsibility.
  std::function<void(const PictureOrderedState&, const Select&)> preselect;
  // Pure live lookups: slot ownership is checked. Missing owner context must
  // throw; only an absent associated view is represented by nullopt.
  std::function<std::size_t(const PictureOrderedState&, std::uint64_t)> slot_of;
  std::function<std::optional<std::uint8_t>(const PictureOrderedState&, std::uint64_t)> view_order;
  std::function<bool(PictureOrderedState&, std::uint32_t)> draw;
  std::function<bool()> special_enabled;
  std::function<std::optional<std::uint64_t>()> special_context;
  // Required admitted profiling/stream-production/begin/items/end service.
  // This boundary is not a fabricated empty special renderer.
  std::function<void(std::uint64_t)> first_round_service;
};

class PictureOrderedCoordinator final {
public:
  PictureOrderedCoordinator() = default;
  PictureOrderedCoordinator(const PictureOrderedCoordinator&) = delete;
  PictureOrderedCoordinator& operator=(const PictureOrderedCoordinator&) = delete;
  PictureOrderedCoordinator(PictureOrderedCoordinator&&) = delete;
  PictureOrderedCoordinator& operator=(PictureOrderedCoordinator&&) = delete;

  [[nodiscard]] bool poisoned() const noexcept { return poisoned_; }
  // Caller supplies the already collected state snapshot and serializes access.
  // State identities/membership, entry storage/record ownership and selected
  // lists must remain stable during callbacks (except bounded preselection).
  // Draw services may update keys/associations and the active cursor, not resize
  // storage. Resolvers use current state; no saved-key restoration is invented.
  // max_rounds is a positive explicit native work bound. Hitting it while work
  // remains aborts, never discards work. A callback/validation failure after
  // entry poisons this coordinator and preserves every completed effect.
  // Failed preflight validation has no effects. No rollback, retry or implicit
  // cleanup is provided after poison; caller must discard/rebuild frame state.
  // Repeated pointers are retained: repeated snapshot slots use the same live
  // cursor/list, not separately copied states. Null state pointers reject.
  [[nodiscard]] std::uint32_t run(std::span<PictureOrderedState*> states,
      std::uint32_t max_rounds, const PictureOrderedCoordinatorHooks& hooks);
private:
  bool running_{false};
  bool poisoned_{false};
};

} // namespace off::graphics
