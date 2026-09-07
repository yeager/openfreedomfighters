#include "off/cutscene/cut_sequence_coordinator.hpp"

#include <stdexcept>

namespace off::cutscene {

CutSequenceCoordinator::CutSequenceCoordinator(std::span<const CutSequenceEntry> entries)
    : entries_(entries.begin(), entries.end()) {
  for (const auto& entry : entries_) {
    if (entry.cut == 0U) throw std::runtime_error("cut sequence entry requires a live cut handle");
  }
}

void CutSequenceCoordinator::start_index(std::size_t index,
                                         const CutSequenceStartServices& services) {
  if (index >= entries_.size() || !services.start_cut || !services.start_parallel_group) {
    throw std::runtime_error("cut sequence start requires an indexed entry and both services");
  }
  active_index_ = index;
  const auto& entry = entries_[index];
  services.start_cut(entry.cut);
  for (const auto group : entry.parallel_groups) {
    if (group == 0U) throw std::runtime_error("cut sequence group requires a live handle");
    services.start_parallel_group(group);
  }
}

void CutSequenceCoordinator::start(std::size_t index, const CutSequenceStartServices& services) {
  if (active_index_) throw std::runtime_error("cut sequence start requires no active cut");
  start_index(index, services);
}

CutSequenceAdvanceResult CutSequenceCoordinator::complete_current(
    const CutSequenceStartServices& services) {
  if (!active_index_) throw std::runtime_error("cut sequence completion requires an active cut");
  const auto next = *active_index_ + 1U;
  active_index_.reset();
  if (next >= entries_.size()) return CutSequenceAdvanceResult::sequence_exhausted;
  start_index(next, services);
  return CutSequenceAdvanceResult::advanced;
}

std::optional<std::size_t> CutSequenceCoordinator::active_index() const noexcept {
  return active_index_;
}

} // namespace off::cutscene
