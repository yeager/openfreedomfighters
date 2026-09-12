#include "off/data/install.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void write(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "fixture";
  if (!output) throw std::runtime_error("could not create executable fixture");
}

}  // namespace

int main() {
  const std::filesystem::path work = OFF_EXECUTABLE_PATH_TEST_WORK_DIR;
  if (work.empty() || work == work.root_path())
    throw std::runtime_error("invalid executable path fixture directory");
  std::error_code error;
  std::filesystem::remove_all(work, error);
  std::filesystem::create_directories(work);

  const auto symlink_root = work / "symlink";
  write(work / "outside" / "Freedom.Exe");
  std::filesystem::create_directories(symlink_root);
  std::filesystem::create_symlink(
      std::filesystem::absolute(work / "outside" / "Freedom.Exe"),
      symlink_root / "Freedom.Exe", error);
  if (!error) {
    const auto result = off::data::verify_install(symlink_root);
    check(result.error == off::data::InstallError::io_error,
          "executable symlink is rejected before its target can be hashed");
  } else {
    std::cout << "SKIP: symbolic-link fixture unavailable: " << error.message() << '\n';
  }

  const auto case_root = work / "case-spellings";
  write(case_root / "Freedom.Exe");
  write(case_root / "Freedom.exe");
  if (!std::filesystem::equivalent(case_root / "Freedom.Exe",
                                   case_root / "Freedom.exe")) {
    const auto result = off::data::verify_install(case_root);
    check(result.error == off::data::InstallError::io_error,
          "distinct executable case spellings are not selected arbitrarily");
  } else {
    std::cout << "SKIP: case-spelling fixture needs a case-sensitive filesystem\n";
  }

  return failures == 0 ? 0 : 1;
}
