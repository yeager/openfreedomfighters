#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace off::graphics {

// Project-owned static identity scope for one selected world-scene package.
// It is not an original engine handle or a gameplay runtime identity.
struct WorldSceneSessionId {
  std::uint64_t value{};
  bool operator==(const WorldSceneSessionId &) const = default;
};

// A source-owner identity is scoped to its package session so the same GMS row
// loaded by two scene sessions cannot alias in future candidate/history maps.
struct WorldSceneOwnerId {
  WorldSceneSessionId session{};
  std::size_t source_directory_index{};
  bool operator==(const WorldSceneOwnerId &) const = default;
};

struct WorldSceneSourceOwner {
  WorldSceneOwnerId id{};
  std::optional<WorldSceneOwnerId> parent_owner;
  std::uint32_t source_type{};
  std::uint32_t local_slot_index{};
};

} // namespace off::graphics
