#include "off/data/install_audit_cache.hpp"

#include <fstream>
#include <string>

namespace off::data {
namespace {

constexpr std::string_view record_prefix = "OFF-INSTALL-AUDIT-CACHE\n1\n";

std::string record_contents(std::string_view identity) {
  return std::string{record_prefix} + std::string{identity} + "\n";
}

std::filesystem::path record_path(const std::filesystem::path& root,
                                  std::string_view identity) {
  return root / (std::string{identity} + ".ok");
}

bool regular_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) &&
         !std::filesystem::is_symlink(status);
}

bool directory_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) &&
         !std::filesystem::is_symlink(status);
}

} // namespace

bool install_audit_cache_hit(const std::filesystem::path& root,
                             std::string_view identity) noexcept {
  try {
    if (root.empty())
      return false;
    const auto record = record_path(root, identity);
    if (!regular_non_link(record))
      return false;
    std::error_code error;
    if (std::filesystem::file_size(record, error) != record_contents(identity).size() ||
        error)
      return false;
    std::ifstream input(record, std::ios::binary);
    std::string contents{std::istreambuf_iterator<char>{input}, {}};
    return input.good() || input.eof()
               ? contents == record_contents(identity)
               : false;
  } catch (...) {
    return false;
  }
}

void store_install_audit_cache(const std::filesystem::path& root,
                               std::string_view identity) noexcept {
  try {
    if (root.empty())
      return;
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error || !directory_non_link(root))
      return;
    const auto record = record_path(root, identity);
    const auto staging = record.string() + ".part";
    if (std::filesystem::exists(record, error) || error)
      return;
    if (std::filesystem::exists(staging, error))
      std::filesystem::remove(staging, error);
    std::ofstream output(staging, std::ios::binary | std::ios::trunc);
    if (!output)
      return;
    output << record_prefix << identity << '\n';
    output.flush();
    if (!output)
      return;
    output.close();
    // The validator above intentionally reads only the final name, so validate
    // the staging bytes before exposing them through the public cache name.
    {
      std::ifstream input(staging, std::ios::binary);
      std::string contents{std::istreambuf_iterator<char>{input}, {}};
      if (contents != record_contents(identity)) {
        std::filesystem::remove(staging, error);
        return;
      }
    }
    std::filesystem::rename(staging, record, error);
    if (error)
      std::filesystem::remove(staging, error);
  } catch (...) {
  }
}

} // namespace off::data
