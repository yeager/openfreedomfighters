#pragma once

#include "off/crypto/sha256.hpp"
#include "off/simulation/world.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::simulation {

// A project-authored, opaque component payload.  Its type and byte contract
// belong to a native system registered by OpenFreedomFighters; it has no
// retail component-format or save compatibility meaning.
struct ComponentRecord final {
  std::uint32_t type{};
  EntityId entity{};
  std::vector<std::byte> payload;
  auto operator<=>(const ComponentRecord &) const = default;
};

struct ComponentStoreLimits final {
  std::uint32_t maximum_records{262'144};
  std::uint32_t maximum_types{4'096};
  std::size_t maximum_payload_bytes{64U * 1024U * 1024U};
  std::size_t maximum_record_bytes{4U * 1024U * 1024U};
  auto operator<=>(const ComponentStoreLimits &) const = default;
};

// Portable, versioned envelope limits.  These limits defend the decode
// boundary; the store's own limits are retained in its canonical payload.
struct ComponentSnapshotReadLimits final {
  std::size_t maximum_bytes{128U * 1024U * 1024U};
  ComponentStoreLimits maximum_store{};
};

// Deterministic component registry for project-owned simulation systems.
// Records are ordered by (type, entity index, entity generation), never by
// pointer or insertion address.  This class intentionally does not decide
// whether an EntityId is live: callers own lifecycle validation at the world
// boundary, allowing a snapshot to remain independently serializable.
class ComponentStore final {
public:
  explicit ComponentStore(ComponentStoreLimits limits = {});

  // Replaces an exact (type, entity) record or inserts it at its canonical
  // location. Type zero and the null EntityId are reserved and rejected.
  void upsert(std::uint32_t type, EntityId entity,
              std::span<const std::byte> payload);
  [[nodiscard]] bool erase(std::uint32_t type, EntityId entity) noexcept;
  void erase_entity(EntityId entity) noexcept;
  void clear() noexcept;

  [[nodiscard]] std::span<const ComponentRecord> records() const noexcept {
    return records_;
  }
  [[nodiscard]] std::span<const std::byte>
  find(std::uint32_t type, EntityId entity) const noexcept;
  [[nodiscard]] std::size_t payload_bytes() const noexcept {
    return payload_bytes_;
  }
  [[nodiscard]] const ComponentStoreLimits &limits() const noexcept {
    return limits_;
  }

  [[nodiscard]] crypto::Sha256Digest state_hash() const;
  [[nodiscard]] std::vector<std::byte> export_snapshot() const;
  void import_snapshot(std::span<const std::byte> bytes,
                       ComponentSnapshotReadLimits limits = {});

private:
  [[nodiscard]] static bool valid_key(std::uint32_t type,
                                      EntityId entity) noexcept;
  [[nodiscard]] static bool key_less(const ComponentRecord &left,
                                     const ComponentRecord &right) noexcept;
  [[nodiscard]] std::vector<ComponentRecord>::iterator
  lower_bound(std::uint32_t type, EntityId entity) noexcept;
  [[nodiscard]] std::vector<ComponentRecord>::const_iterator
  lower_bound(std::uint32_t type, EntityId entity) const noexcept;
  void validate_payload_size(std::size_t bytes) const;

  ComponentStoreLimits limits_;
  std::size_t payload_bytes_{};
  std::vector<ComponentRecord> records_;
};

} // namespace off::simulation
