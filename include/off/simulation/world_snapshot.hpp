#pragma once

#include <cstddef>
#include <cstdint>

namespace off::simulation {

// Project-authored limits for the portable SimulationWorld snapshot format.
// These are not retail save-file limits or compatibility claims.
struct SnapshotReadLimits {
  std::size_t maximum_bytes{64U * 1024U * 1024U};
  std::uint32_t maximum_entities{1'000'000U};
  std::uint32_t maximum_pending_spawns{1'000'000U};
  std::uint32_t maximum_pending_destroys{1'000'000U};
  std::uint32_t maximum_pending_events{1'000'000U};
};

} // namespace off::simulation
