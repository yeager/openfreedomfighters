#include "off/simulation/save_store.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures{};
void check(bool condition, const char *message) { if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; } }
off::simulation::InputSnapshot input(std::uint64_t tick) { return {.tick = tick}; }
off::simulation::ProjectSaveIdentity identity() {
  off::simulation::ProjectSaveIdentity result;
  for (std::size_t index = 0; index < result.data_manifest_fingerprint.size(); ++index)
    result.data_manifest_fingerprint[index] = static_cast<std::uint8_t>(index + 1);
  result.campaign_id = "project-campaign.1";
  return result;
}
}

int main() {
  using namespace off::simulation;
  const auto root = std::filesystem::current_path() / ("off-save-store-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const ProjectSaveStore store(root / "campaign");
  const auto expected_identity = identity();
  SimulationWorld original;
  check(store.load(original, expected_identity).status == SaveLoadStatus::missing,
        "empty two-generation store reports missing");
  check(!store.save(original, {.campaign_id = "bad/name"}),
        "invalid project campaign identity never creates a save");
  static_cast<void>(original.queue_spawn({{1, 2, 3}, 9}));
  static_cast<void>(original.step(input(1)));
  const auto first_hash = original.state_hash();
  check(store.save(original, expected_identity), "write first portable generation");
  check(std::filesystem::exists(store.generation_path(0)) && !std::filesystem::exists(store.generation_path(1)),
        "first write owns only one same-directory generation");
  static_cast<void>(original.queue_spawn({{4, 5, 6}, 10}));
  static_cast<void>(original.step(input(2)));
  const auto second_hash = original.state_hash();
  check(store.save(original, expected_identity), "write second portable generation");
  check(std::filesystem::exists(store.generation_path(0)) && std::filesystem::exists(store.generation_path(1)),
        "second write retains an independent previous generation");
  SimulationWorld restored;
  const auto loaded = store.load(restored, expected_identity);
  check(loaded.status == SaveLoadStatus::loaded && loaded.generation == 2 && restored.state_hash() == second_hash,
        "recovery selects unambiguously newest valid generation");
  {
    std::ofstream corrupt(store.generation_path(1), std::ios::binary | std::ios::trunc);
    corrupt << "corrupt";
  }
  SimulationWorld recovered;
  const auto recovered_result = store.load(recovered, expected_identity);
  check(recovered_result.status == SaveLoadStatus::loaded && recovered_result.generation == 1 && recovered.state_hash() == first_hash,
        "corrupt newest generation recovers the older fully validated world");
  const auto before_wrong_identity = recovered.state_hash();
  auto wrong_identity = expected_identity;
  wrong_identity.campaign_id = "other-campaign";
  check(store.load(recovered, wrong_identity).status == SaveLoadStatus::invalid && recovered.state_hash() == before_wrong_identity,
        "identity mismatch never mutates live simulation state");
  check(store.save(original, expected_identity), "replace invalid generation with a later durable generation");
  const auto current = store.load(recovered, expected_identity);
  check(current.status == SaveLoadStatus::loaded && current.generation == 2 && recovered.state_hash() == second_hash,
        "writing selects invalid generation before discarding the valid predecessor");
  {
    std::error_code error;
    std::filesystem::copy_file(store.generation_path(0), store.generation_path(1), std::filesystem::copy_options::overwrite_existing, error);
    check(!error, "duplicate valid generation for ambiguity coverage");
  }
  const auto before_ambiguous = recovered.state_hash();
  check(store.load(recovered, expected_identity).status == SaveLoadStatus::ambiguous && recovered.state_hash() == before_ambiguous,
        "equal valid generations are rejected without guessing newest state");
  check(!store.save(original, expected_identity), "ambiguous equal generations are never overwritten blindly");
#ifndef _WIN32
  {
    const auto target = root / "save-target";
    std::ofstream output(target, std::ios::binary); output << "unmodified"; output.close();
    std::error_code error;
    const ProjectSaveStore linked(root / "linked-campaign");
    std::filesystem::create_symlink(target.filename(), linked.generation_path(0), error);
    check(!error && !linked.save(original, expected_identity), "save never follows a generation leaf symlink");
    SimulationWorld untouched;
    const auto untouched_hash = untouched.state_hash();
    check(linked.load(untouched, expected_identity).status == SaveLoadStatus::invalid &&
              untouched.state_hash() == untouched_hash,
          "load rejects a generation leaf symlink without following or mutating state");
  }
#endif
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
