#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace off::cutscene {

struct CutSequenceEntry {
  std::uint64_t cut{};
  std::vector<std::uint64_t> parallel_groups;
};

enum class CutSequenceAdvanceResult { advanced, sequence_exhausted };

struct CutSequenceStartServices {
  std::function<void(std::uint64_t)> start_cut;
  std::function<void(std::uint64_t)> start_parallel_group;
};

// An admitted MovieControl response to CutSequence_End, not a scene player.
// Terminal destination remains a separate, unrecovered service.
class CutSequenceCoordinator final {
public:
  explicit CutSequenceCoordinator(std::span<const CutSequenceEntry> entries);
  CutSequenceCoordinator(const CutSequenceCoordinator&) = delete;
  CutSequenceCoordinator& operator=(const CutSequenceCoordinator&) = delete;
  CutSequenceCoordinator(CutSequenceCoordinator&&) = delete;
  CutSequenceCoordinator& operator=(CutSequenceCoordinator&&) = delete;

  void start(std::size_t index, const CutSequenceStartServices& services);
  [[nodiscard]] CutSequenceAdvanceResult complete_current(
      const CutSequenceStartServices& services);
  [[nodiscard]] std::optional<std::size_t> active_index() const noexcept;

private:
  void start_index(std::size_t index, const CutSequenceStartServices& services);

  std::vector<CutSequenceEntry> entries_;
  std::optional<std::size_t> active_index_;
};

} // namespace off::cutscene
