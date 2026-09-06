#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace off::graphics {

struct PictureOrderedDrawEntry {
  std::uint32_t key;
  std::uint64_t record_identity;
  // Retained record -> owner context -> admitted view, never a decoded key index.
  std::optional<std::uint64_t> associated_view;
  std::optional<std::uint64_t> resource;
};
struct PictureOrderedDrawHooks {
  // Concrete ordinary backend/material reset, including on exhausted rounds.
  std::function<void()> reset;
  std::function<void(std::uint64_t)> view_transition;
  std::function<void(std::uint8_t)> subtype_begin;
  std::function<void(std::uint8_t)> subtype_end;
  std::function<void(std::uint64_t)> bind_resource;
  std::function<void(std::uint64_t, std::uint8_t)> emit;
};

class PictureOrderedDrawLoop final {
public:
  PictureOrderedDrawLoop() = default;
  PictureOrderedDrawLoop(const PictureOrderedDrawLoop&) = delete;
  PictureOrderedDrawLoop& operator=(const PictureOrderedDrawLoop&) = delete;
  PictureOrderedDrawLoop(PictureOrderedDrawLoop&&) = delete;
  PictureOrderedDrawLoop& operator=(PictureOrderedDrawLoop&&) = delete;

  // Entries and cursor must already reflect actual outer selection/rebuild.
  // No automatic cursor reset, association repair, sorting or admission.
  // Returns whether entries remain after this invocation (barrier rounds).
  // Stable entries/referenced lifetimes and no callback mutation are required.
  // Native policy: validate every remaining nonbarrier/nonreserved association,
  // including equal-key entries and entries beyond a barrier. Invalid cursor,
  // sentinel collision, missing live association,
  // incomplete hooks and reentry reject before effects. Callback exceptions
  // preserve their prefix; abort the frame, never retry as a transaction.
  [[nodiscard]] bool run(std::span<const PictureOrderedDrawEntry> entries,
                         std::size_t& prepared_cursor,
                         const PictureOrderedDrawHooks& hooks);
private:
  bool running_{false};
};

} // namespace off::graphics
