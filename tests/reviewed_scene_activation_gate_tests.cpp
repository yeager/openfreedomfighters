#include "off/runtime/reviewed_scene_activation_gate.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void write(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  check(static_cast<bool>(output), "fixture write succeeds");
}
// Authored categorical fixture, never a retail observation or game-data sample.
std::string contract() {
  const std::string success = R"({"manager_entered":true,"scene_staged":true,"global_lifecycle_completed":true,"component_phase_one_completed":true,"camera_route_ready":true,"scene_committed":true,"outcome":"success"})";
  const std::string failure = R"({"manager_entered":true,"scene_staged":false,"global_lifecycle_completed":false,"component_phase_one_completed":false,"camera_route_ready":false,"scene_committed":false,"outcome":"failure"})";
  return R"({"format":"off.scene-manager-lifecycle-contract/v1","candidate":)" + success +
      R"(,"repeat":)" + success + R"(,"failure":)" + failure + "}";
}
}  // namespace

int main() {
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR};
    const auto directory = root / "reviewed-scene-activation-gate";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    write(directory / "reviewed-scene-manager-lifecycle.json", contract());
    const auto receipt = off::runtime::ReviewedSceneManagerLifecycleContract::load_local(directory);
    check(receipt.has_value(), "reviewed lifecycle fixture is admitted");

    const auto staged = off::runtime::StagedSceneActivationCandidate::from_staged_lease(
        std::make_shared<const int>(7));
    check(staged.has_value(), "non-null staged lease forms a candidate");
    check(!off::runtime::StagedSceneActivationCandidate::from_staged_lease({}),
          "null staged lease is rejected");

    off::runtime::ReviewedSceneActivationGate gate;
    off::runtime::ReviewedSceneActivationState state;
    check(gate.consume(*receipt, *staged, state, {}) ==
              off::runtime::ReviewedSceneActivationResult::rejected &&
              !state.has_committed_candidate() && !gate.active(),
          "missing behavior-specific commit service fails closed");

    std::size_t attempted{};
    const off::runtime::ReviewedSceneActivationServices reject_services{
        .commit_staged_activation = [&] {
          ++attempted;
          return false;
        },
    };
    check(gate.consume(*receipt, *staged, state, reject_services) ==
              off::runtime::ReviewedSceneActivationResult::rejected && attempted == 1U &&
              !state.has_committed_candidate() && !gate.active(),
          "failed commit preserves uncommitted state");

    bool recursive_rejected = false;
    const off::runtime::ReviewedSceneActivationServices recursive_services{
        .commit_staged_activation = [&] {
          try {
            static_cast<void>(gate.consume(*receipt, *staged, state, {}));
          } catch (const std::runtime_error&) {
            recursive_rejected = true;
          }
          return true;
        },
    };
    check(gate.consume(*receipt, *staged, state, recursive_services) ==
              off::runtime::ReviewedSceneActivationResult::committed && recursive_rejected &&
              state.has_committed_candidate() && !gate.active(),
          "one successful explicit commit is retained and recursive use is rejected");
    check(gate.consume(*receipt, *staged, state, recursive_services) ==
              off::runtime::ReviewedSceneActivationResult::rejected,
          "a committed state cannot be replaced implicitly");

    off::runtime::ReviewedSceneActivationGate throwing_gate;
    off::runtime::ReviewedSceneActivationState throwing_state;
    bool propagated = false;
    try {
      static_cast<void>(throwing_gate.consume(
          *receipt, *staged, throwing_state,
          {.commit_staged_activation = []() -> bool {
            throw std::runtime_error("commit failure");
          }}));
    } catch (const std::runtime_error&) {
      propagated = true;
    }
    check(propagated && !throwing_gate.active() && !throwing_state.has_committed_candidate(),
          "throwing commit preserves state and clears the guard");
    std::filesystem::remove_all(root, error);
    std::cout << "reviewed scene activation gate tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
