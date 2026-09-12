#include "off/simulation/component_world.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

std::vector<std::byte> bytes(std::initializer_list<unsigned char> values) {
  std::vector<std::byte> result;
  for (const auto value : values)
    result.push_back(static_cast<std::byte>(value));
  return result;
}

off::simulation::InputSnapshot input(std::uint64_t tick) {
  return {.tick = tick};
}
} // namespace

int main() {
  using namespace off::simulation;

  ComponentWorld bridge;
  const auto request = bridge.world().queue_spawn({{4, 5, 6}, 7});
  const auto spawned = bridge.step(input(1));
  check(spawned.spawned.size() == 1 && spawned.spawned.front().request_id == request,
        "bridge preserves the world spawn result");
  const auto entity = spawned.spawned.front().entity;
  const auto payload = bytes({1, 2, 3});
  bridge.upsert_component(12, entity, payload);
  check(std::ranges::equal(bridge.components().find(12, entity), payload),
        "live entity accepts its opaque project component");

  bool rejected = false;
  try {
    bridge.upsert_component(12, {entity.index, entity.generation + 1}, payload);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "stale entity generation never receives a component");

  bridge.world().queue_destroy(entity);
  const auto destroyed = bridge.step(input(2));
  check(destroyed.destroyed == std::vector<EntityId>{entity} &&
            bridge.components().find(12, entity).empty(),
        "destroyed entity components are retired at the same bridge step");

  const auto second_request = bridge.world().queue_spawn({{7, 8, 9}, 10});
  const auto reused = bridge.step(input(3));
  const auto replacement = reused.spawned.front().entity;
  check(reused.spawned.front().request_id == second_request &&
            replacement.index == entity.index && replacement.generation != entity.generation &&
            bridge.components().records().empty(),
        "slot reuse cannot expose components from the prior generation");

  ComponentWorld equivalent;
  static_cast<void>(equivalent.world().queue_spawn({{4, 5, 6}, 7}));
  const auto equivalent_entity = equivalent.step(input(1)).spawned.front().entity;
  equivalent.upsert_component(12, equivalent_entity, payload);
  equivalent.world().queue_destroy(equivalent_entity);
  static_cast<void>(equivalent.step(input(2)));
  static_cast<void>(equivalent.world().queue_spawn({{7, 8, 9}, 10}));
  static_cast<void>(equivalent.step(input(3)));
  check(bridge.state_hash() == equivalent.state_hash(),
        "bridge checkpoint is deterministic across equal world/component history");

  bridge.reset();
  check(bridge.world().tick() == 0 && bridge.components().records().empty(),
        "reset clears both project-owned state layers");
  return EXIT_SUCCESS;
}
