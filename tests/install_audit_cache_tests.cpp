#include "off/data/install_audit_cache.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void write(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!output) throw std::runtime_error("could not create cache fixture");
}
}

int main() {
  const std::filesystem::path root = OFF_AUDIT_CACHE_TEST_WORK_DIR;
  if (root.empty() || root == root.root_path())
    throw std::runtime_error("invalid cache fixture directory");
  std::filesystem::remove_all(root);
  constexpr std::string_view identity =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  check(!off::data::install_audit_cache_hit({}, identity),
        "an empty cache root is disabled");
  check(!off::data::install_audit_cache_hit(root, identity),
        "a missing entry is a cache miss");
  off::data::store_install_audit_cache(root, identity);
  check(off::data::install_audit_cache_hit(root, identity),
        "a complete stored entry is a cache hit");
  check(!off::data::install_audit_cache_hit(root, "different-identity"),
        "entries cannot be reused for another identity");
  const auto record = root / (std::string{identity} + ".ok");
  write(record, "truncated");
  check(!off::data::install_audit_cache_hit(root, identity),
        "truncated records safely miss");
  write(record, "OFF-INSTALL-AUDIT-CACHE\n999\n" + std::string{identity} + "\n");
  check(!off::data::install_audit_cache_hit(root, identity),
        "unknown record versions safely miss");
  write(record, "OFF-INSTALL-AUDIT-CACHE\n1\n" + std::string{identity} + "\ntrailing");
  check(!off::data::install_audit_cache_hit(root, identity),
        "trailing bytes safely miss");
  off::data::store_install_audit_cache(root, identity);
  check(!off::data::install_audit_cache_hit(root, identity),
        "a corrupt existing record is never trusted or overwritten");
  std::filesystem::remove(record);
  off::data::store_install_audit_cache(root, identity);
  check(off::data::install_audit_cache_hit(root, identity),
        "a removed corrupt record can be safely rebuilt");
  std::filesystem::remove_all(root);
  return failures == 0 ? 0 : 1;
}
