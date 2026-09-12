#include "off/graphics/reviewed_startup_coordinator_pass_contract.hpp"

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
// Authored structural fixture; this is not an observation or retail data.
std::string contract() {
  const std::string success = R"({"pass_order":0,"coordinator_constructed":true,"manager_constructed":true,"pass_entered":true,"work_list_ready":true,"selected_work":"startup_picture","work_admission":"admitted","pass_completed":true,"outcome":"success","external_service":"entered"})";
  const std::string failure = R"({"pass_order":0,"coordinator_constructed":true,"manager_constructed":true,"pass_entered":true,"work_list_ready":true,"selected_work":"startup_picture","work_admission":"rejected","pass_completed":false,"outcome":"failure","external_service":"entered"})";
  return R"({"format":"off.startup-coordinator-pass-contract/v1","candidate":)" + success +
      R"(,"repeat":)" + success + R"(,"failure":)" + failure + "}";
}
}  // namespace

int main() {
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR};
    const auto directory = root / "reviewed-startup-coordinator-pass";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    const auto path = directory / "reviewed-startup-coordinator-pass.json";
    write(path, contract());
    const auto admitted = off::graphics::ReviewedStartupCoordinatorPassContract::load_local(directory);
    check(admitted && admitted->admitted(), "exact source-free pass contract is admitted inertly");
    write(path, contract() + std::string(1, '\0'));
    check(!off::graphics::ReviewedStartupCoordinatorPassContract::load_local(directory),
          "trailing bytes are rejected");
    write(path, R"({"format":"off.startup-coordinator-pass-contract/v1","candidate":{},"repeat":{},"failure":{}})");
    check(!off::graphics::ReviewedStartupCoordinatorPassContract::load_local(directory),
          "incomplete records are rejected");
    write(path, contract());
    const auto linked = root / "linked-contract.json";
    std::filesystem::rename(path, linked, error);
    check(!error, "fixture moves");
    std::filesystem::create_symlink(linked, path, error);
    check(!error, "symlink fixture exists");
    check(!off::graphics::ReviewedStartupCoordinatorPassContract::load_local(directory),
          "symlink is rejected");
    std::filesystem::remove_all(root, error);
    std::cout << "reviewed startup coordinator pass contract tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
