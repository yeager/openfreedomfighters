#include "off/simulation/component_world.hpp"

#include <array>
#include <stdexcept>

namespace off::simulation {

ComponentWorld::ComponentWorld(WorldLimits world_limits,
                               ComponentStoreLimits component_limits)
    : world_(world_limits), components_(component_limits) {}

void ComponentWorld::upsert_component(std::uint32_t type, EntityId entity,
                                      std::span<const std::byte> payload) {
  if (!world_.alive(entity))
    throw std::invalid_argument("component owner is not a live world entity");
  components_.upsert(type, entity, payload);
}

bool ComponentWorld::erase_component(std::uint32_t type,
                                     EntityId entity) noexcept {
  return components_.erase(type, entity);
}

std::uint64_t ComponentWorld::queue_spawn(SpawnState state) {
  return world_.queue_spawn(state);
}

void ComponentWorld::queue_destroy(EntityId entity) {
  world_.queue_destroy(entity);
}

std::uint64_t ComponentWorld::queue_event(std::uint64_t tick,
                                          std::uint32_t type,
                                          EntityId source, EntityId target,
                                          std::int64_t value) {
  return world_.queue_event(tick, type, source, target, value);
}

WorldStepResult ComponentWorld::step(const InputSnapshot &input) {
  auto result = world_.step(input);
  for (const auto entity : result.destroyed)
    components_.erase_entity(entity);
  return result;
}

void ComponentWorld::reset() noexcept {
  world_.reset();
  components_.clear();
}

crypto::Sha256Digest ComponentWorld::state_hash() const {
  constexpr std::array<std::uint8_t, 8> magic{'O', 'F', 'F', 'C', 'W', 1, 0,
                                               0};
  const auto world_hash = world_.state_hash();
  const auto component_hash = components_.state_hash();
  crypto::Sha256 hash;
  hash.update({reinterpret_cast<const std::byte *>(magic.data()), magic.size()});
  hash.update({reinterpret_cast<const std::byte *>(world_hash.data()),
               world_hash.size()});
  hash.update({reinterpret_cast<const std::byte *>(component_hash.data()),
               component_hash.size()});
  return hash.finish();
}

} // namespace off::simulation
