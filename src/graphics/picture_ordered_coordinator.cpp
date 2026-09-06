#include "off/graphics/picture_ordered_coordinator.hpp"

#include <stdexcept>
#include <memory>

namespace off::graphics {

std::uint32_t PictureOrderedCoordinator::run(std::span<PictureOrderedState*> states,
    std::uint32_t max_rounds, const PictureOrderedCoordinatorHooks& hooks) {
  if (running_ || poisoned_)
    throw std::runtime_error("ordered coordinator is active or poisoned");
  if (max_rounds == 0 || !hooks.preselect || !hooks.slot_of || !hooks.view_order ||
      !hooks.draw || !hooks.special_enabled || !hooks.special_context || !hooks.first_round_service)
    throw std::runtime_error("ordered coordinator requires complete hooks and a positive round bound");
  for (const auto* state : states)
    if (!state) throw std::runtime_error("ordered coordinator requires live state pointers");

  struct Guard {
    bool& flag;
    explicit Guard(bool& active) : flag(active) { flag = true; }
    ~Guard() { flag = false; }
  } guard(running_);
  const auto slot = [&](const PictureOrderedState& state, std::uint64_t record) {
    const auto index = hooks.slot_of(state, record);
    if (index >= state.entries.size() || state.entries[index].record_identity != record)
      throw std::runtime_error("selected record does not own its ordered slot");
    return index;
  };
  try {
    for (auto* current : states) {
      auto& state = *current;
      state.selected_records.clear();
      struct Selection {
        PictureOrderedState* state;
        bool active{true};
      };
      const auto selection = std::make_shared<Selection>(Selection{&state});
      struct CloseSelection {
        std::shared_ptr<Selection> selection;
        ~CloseSelection() { selection->active = false; }
      };
      {
        const CloseSelection close{selection};
        hooks.preselect(state, [selection](std::uint64_t record) {
          if (!selection->active)
            throw std::runtime_error("ordered selection visitor is no longer active");
          auto& selected = selection->state->selected_records;
          if (selected.size() >= 8192)
            throw std::runtime_error("ordered preselection capacity exceeded");
          selected.push_back(record);
        });
      }
      for (const auto record : state.selected_records)
        state.entries[slot(state, record)].key |= 0x78000000U;
      state.cursor = 0;
    }
    std::uint32_t round = 0;
    for (;;) {
      bool more = false;
      for (auto* current : states) {
        auto& state = *current;
        if (!state.cursor || *state.cursor > state.entries.size())
          throw std::runtime_error("ordered draw requires an active in-range cursor");
        const bool state_more = hooks.draw(state, round);
        if (!state.cursor || *state.cursor > state.entries.size())
          throw std::runtime_error("ordered draw produced an invalid cursor");
        more = more || state_more;
      }
      ++round;
      if (round == 1 && hooks.special_enabled()) {
        if (const auto context = hooks.special_context()) hooks.first_round_service(*context);
      }
      if (!more) break;
      if (round == max_rounds)
        throw std::runtime_error("ordered drawing exceeded its explicit round bound");
    }
    for (auto* current : states) {
      auto& state = *current;
      state.cursor.reset();
      for (const auto record : state.selected_records) {
        auto& key = state.entries[slot(state, record)].key;
        key &= 0x87ffffffU;
        const auto order = hooks.view_order(state, record).value_or(0);
        key |= static_cast<std::uint32_t>(order) << 27U;
      }
    }
    return round;
  } catch (...) {
    poisoned_ = true;
    throw;
  }
}

} // namespace off::graphics
