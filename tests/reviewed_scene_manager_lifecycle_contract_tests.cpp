#include "off/runtime/reviewed_scene_manager_lifecycle_contract.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
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
// Authored structural fixture, never a retail observation or game-data sample.
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
    const auto directory = root / "reviewed-scene-manager-lifecycle";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    const auto path = directory / "reviewed-scene-manager-lifecycle.json";
    write(path, contract());
    const auto admitted = off::runtime::ReviewedSceneManagerLifecycleContract::load_local(directory);
    check(admitted && admitted->admitted(), "exact source-free lifecycle receipt is admitted inertly");
    check(admitted->receipt() == off::runtime::SceneManagerLifecycleReceipt::repeated_success_with_distinct_failure,
          "only the categorical receipt is retained");
    write(path, contract() + std::string(1, '\0'));
    check(!off::runtime::ReviewedSceneManagerLifecycleContract::load_local(directory),
          "trailing bytes are rejected");
    write(path, R"({"format":"off.scene-manager-lifecycle-contract/v1","candidate":{},"repeat":{},"failure":{},"extra":false})");
    check(!off::runtime::ReviewedSceneManagerLifecycleContract::load_local(directory),
          "extra or incomplete fields are rejected");
    write(path, contract());
    const auto linked = root / "linked-contract.json";
    std::filesystem::rename(path, linked, error);
    check(!error, "fixture moves");
    std::filesystem::create_symlink(linked, path, error);
    check(!error, "symlink fixture exists");
    check(!off::runtime::ReviewedSceneManagerLifecycleContract::load_local(directory),
          "symlink receipt is rejected");
    std::filesystem::remove_all(root, error);
    std::cout << "reviewed scene manager lifecycle contract tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
