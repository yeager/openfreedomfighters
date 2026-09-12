#include "off/data/install_manifest.hpp"
#include "off/crypto/sha256.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace off::data {
namespace {
constexpr ManifestFile reference_files[]{
#include "install_manifest_reference.inc"
};
std::string folded(std::string_view path) {
  std::string result(path);
  for (auto& c : result) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return result;
}
bool soundtrack_path(std::string_view p) {
  return p == "freedom_fighters_ost" || p.starts_with("freedom_fighters_ost/");
}
bool game_path(std::string_view p) { return p == "scenes" || p.starts_with("scenes/"); }
bool ignored_cache(std::string_view p) {
  return p == "freedom_fighters_ost/ff_ost_mp3_320/thumbs.db";
}
bool safe(std::string_view p) {
  return is_safe_archive_path(p) && p.find('\\') == std::string_view::npos;
}
void validate_manifest(std::span<const ManifestFile> manifest) {
  std::unordered_map<std::string, const ManifestFile*> expected;
  for (const auto& entry : manifest) {
    if (!safe(entry.path) || entry.sha256.size() != 64 ||
        !std::all_of(entry.sha256.begin(), entry.sha256.end(), [](char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }) || !expected.emplace(folded(entry.path), &entry).second)
      throw std::runtime_error("invalid or duplicate file-manifest entry");
    if (entry.role != ManifestFileRole::required_game &&
        entry.role != ManifestFileRole::optional_soundtrack &&
        entry.role != ManifestFileRole::optional_support)
      throw std::runtime_error("invalid file-manifest role");
  }
  for (const auto& entry : manifest) {
    const auto key = folded(entry.path);
    for (auto slash = key.find('/'); slash != std::string::npos;
         slash = key.find('/', slash + 1)) {
      if (expected.contains(key.substr(0, slash)))
        throw std::runtime_error("manifest file is also a parent directory");
    }
  }
}
struct Located {
  std::filesystem::path path;
  std::filesystem::file_status status;
  bool duplicate{false};
};

struct HashTask {
  std::size_t check_index;
  const ManifestFile *entry;
  std::filesystem::path path;
  std::uintmax_t size;
  std::filesystem::file_time_type time;
};

constexpr std::size_t default_hash_workers = 2;
constexpr std::size_t maximum_hash_workers = 4;
bool regular_chain(const std::filesystem::path& root, const std::filesystem::path& path) {
  auto current = root;
  const auto relative = path.lexically_relative(root);
  for (const auto& part : relative) {
    current /= part;
    const auto status = std::filesystem::symlink_status(current);
    if (std::filesystem::is_symlink(status)) return false;
    if (current == path) return std::filesystem::is_regular_file(status);
    if (!std::filesystem::is_directory(status)) return false;
  }
  return false;
}
}

std::span<const ManifestFile> supported_install_manifest() noexcept { return reference_files; }

std::string verified_data_manifest_fingerprint(
    std::span<const ManifestFile> manifest) {
  validate_manifest(manifest);
  struct RequiredEntry {
    std::string path;
    const ManifestFile* file;
  };
  std::vector<RequiredEntry> required;
  required.reserve(manifest.size());
  for (const auto& file : manifest) {
    if (file.role == ManifestFileRole::required_game)
      required.push_back({folded(file.path), &file});
  }
  if (required.empty())
    throw std::runtime_error("verified-data fingerprint requires required game files");
  std::ranges::sort(required, {}, &RequiredEntry::path);
  std::string input{"openfreedomfighters-verified-data-manifest-v1\n"};
  for (const auto& entry : required) {
    input += entry.path;
    input += ':';
    input += std::to_string(entry.file->size);
    input += ':';
    input += entry.file->sha256;
    input += '\n';
  }
  return crypto::to_hex(crypto::sha256(input));
}

std::string supported_data_manifest_fingerprint() {
  return verified_data_manifest_fingerprint(supported_install_manifest());
}

bool ManifestVerification::required_ok() const noexcept {
  return !cancelled && std::none_of(files.begin(), files.end(), [](const auto& f) {
    return f.role == ManifestFileRole::required_game && f.status != ManifestFileStatus::verified;
  });
}

ManifestVerification verify_file_manifest(const std::filesystem::path& requested_root,
    std::span<const ManifestFile> manifest, const std::function<bool()>& cancelled,
    std::size_t hash_workers) {
  validate_manifest(manifest);
  std::unordered_map<std::string, const ManifestFile*> expected;
  for (const auto& entry : manifest)
    expected.emplace(folded(entry.path), &entry);
  std::unordered_map<std::string, ManifestFileRole> ancestors;
  for (const auto& entry : manifest) {
    const auto key = folded(entry.path);
    for (auto slash = key.find('/'); slash != std::string::npos; slash = key.find('/', slash + 1)) {
      const auto parent = key.substr(0, slash);
      auto [found, inserted] = ancestors.emplace(parent, entry.role);
      if (!inserted && (entry.role == ManifestFileRole::required_game ||
          (entry.role == ManifestFileRole::optional_soundtrack && found->second != ManifestFileRole::required_game)))
        found->second = entry.role;
    }
  }
  const auto role_for = [&](const std::string& key) {
    if (const auto entry = expected.find(key); entry != expected.end()) return entry->second->role;
    if (game_path(key)) return ManifestFileRole::required_game;
    if (soundtrack_path(key)) return ManifestFileRole::optional_soundtrack;
    if (const auto parent = ancestors.find(key); parent != ancestors.end()) return parent->second;
    return ManifestFileRole::required_game;
  };
  const auto root = std::filesystem::canonical(requested_root);
  if (!std::filesystem::is_directory(root)) throw std::runtime_error("manifest root is not a directory");
  ManifestVerification result;
  // The cancellation callback predates parallel hashing and callers are not
  // required to make it reentrant.  Serialize calls while latching its result.
  std::atomic_bool cancelled_latched{false};
  std::mutex cancellation_callback_mutex;
  const auto stop = [&] {
    if (cancelled_latched.load(std::memory_order_acquire)) return true;
    if (!cancelled) return false;
    std::lock_guard lock(cancellation_callback_mutex);
    if (!cancelled_latched.load(std::memory_order_relaxed) && cancelled())
      cancelled_latched.store(true, std::memory_order_release);
    return cancelled_latched.load(std::memory_order_acquire);
  };
  std::unordered_map<std::string, Located> located;
  std::vector<std::filesystem::path> directories{root};
  while (!directories.empty()) {
    if (stop()) { result.cancelled = true; return result; }
    const auto directory = directories.back(); directories.pop_back();
    const auto directory_key = folded(directory.lexically_relative(root).generic_string());
    if (const auto found = located.find(directory_key); found != located.end() && found->second.duplicate)
      continue; // Neither spelling of an ambiguous parent supplies verified descendants.
    std::error_code ec;
    std::filesystem::directory_iterator iterator(directory, ec), end;
    if (ec) {
      result.files.push_back({directory_key, directory,
          role_for(directory_key),
          ManifestFileStatus::io_error, "could not list directory"});
      continue;
    }
    for (; iterator != end; iterator.increment(ec)) {
      if (ec) break;
      if (stop()) { result.cancelled = true; return result; }
      const auto path = iterator->path();
      const auto relative = path.lexically_relative(root).generic_string();
      const auto key = folded(relative);
      if (ignored_cache(key)) continue;
      const bool relevant = expected.contains(key) || ancestors.contains(key) || game_path(key) || soundtrack_path(key);
      if (!relevant) continue;
      const auto role = role_for(key);
      const auto status = iterator->symlink_status(ec);
      if (ec || !safe(relative)) {
        result.files.push_back({relative, path, role, ManifestFileStatus::unsafe, "unsafe or unreadable path"});
        ec.clear(); continue;
      }
      auto [found, inserted] = located.emplace(key, Located{path, status});
      if (!inserted) {
        found->second.duplicate = true;
        result.files.push_back({relative, path, role, ManifestFileStatus::ambiguous, "case-insensitive path collision"});
      }
      if (std::filesystem::is_symlink(status)) {
        result.files.push_back({relative, path, role, ManifestFileStatus::unsafe, "symbolic link is not a verified source"});
      } else if (std::filesystem::is_directory(status)) {
        directories.push_back(path);
      } else if (!std::filesystem::is_regular_file(status)) {
        result.files.push_back({relative, path, role, ManifestFileStatus::unsafe, "not a regular file"});
      } else if (!expected.contains(key)) {
        result.files.push_back({relative, path, role, ManifestFileStatus::unexpected, "unrecognized file; not enrolled or selected"});
      }
    }
    if (ec) result.files.push_back({directory_key, directory,
        role_for(directory_key),
        ManifestFileStatus::io_error, "directory enumeration failed"});
  }
  std::vector<ManifestFileCheck> checks;
  checks.reserve(manifest.size());
  std::vector<HashTask> hash_tasks;
  hash_tasks.reserve(manifest.size());
  for (const auto& entry : manifest) {
    if (stop()) { result.cancelled = true; return result; }
    ManifestFileCheck check{std::string(entry.path), {}, entry.role, ManifestFileStatus::missing, "file is absent"};
    const auto key = folded(entry.path);
    bool ambiguous_chain = false;
    for (auto end = key.find('/'); ; end = key.find('/', end + 1)) {
      const auto component = located.find(key.substr(0, end));
      if (component != located.end() && component->second.duplicate) ambiguous_chain = true;
      if (end == std::string::npos) break;
    }
    if (ambiguous_chain) {
      check.status = ManifestFileStatus::ambiguous;
      check.detail = "case-insensitive collision in file or parent path";
      checks.push_back(std::move(check));
      continue;
    }
    const auto found = located.find(key);
    if (found != located.end()) {
      check.actual_path = found->second.path;
      if (found->second.duplicate) {
        check.status = ManifestFileStatus::ambiguous; check.detail = "case-insensitive path collision";
      } else try {
        if (!regular_chain(root, check.actual_path)) {
          check.status = ManifestFileStatus::unsafe; check.detail = "not a regular file with safe parent directories";
        } else {
          const auto size = std::filesystem::file_size(check.actual_path);
          const auto time = std::filesystem::last_write_time(check.actual_path);
          if (size != entry.size) {
            check.status = ManifestFileStatus::size_mismatch;
            check.detail = "expected " + std::to_string(entry.size) + " bytes, found " + std::to_string(size);
          } else {
            hash_tasks.push_back({checks.size(), &entry, check.actual_path, size, time});
          }
        }
      } catch (const std::exception& error) {
        if (stop()) { result.cancelled = true; return result; }
        check.status = ManifestFileStatus::io_error; check.detail = error.what();
      }
    }
    checks.push_back(std::move(check));
  }

  const auto initial_checks = checks;
  const auto hash_one = [&](const HashTask& task) {
    auto& check = checks[task.check_index];
    try {
      const auto digest = crypto::to_hex(crypto::sha256_file(task.path, stop));
      if (!regular_chain(root, task.path) ||
          task.size != std::filesystem::file_size(task.path) ||
          task.time != std::filesystem::last_write_time(task.path)) {
        check.status = ManifestFileStatus::io_error;
        check.detail = "file changed during verification";
      } else if (digest != task.entry->sha256) {
        check.status = ManifestFileStatus::hash_mismatch;
        check.detail = "SHA-256 mismatch: expected " + std::string(task.entry->sha256) +
                       ", found " + digest;
      } else {
        check.status = ManifestFileStatus::verified;
        check.detail.clear();
      }
    } catch (const std::exception& error) {
      if (!stop()) {
        check.status = ManifestFileStatus::io_error;
        check.detail = error.what();
      }
    }
  };
  const auto run_serial = [&] {
    for (const auto& task : hash_tasks) {
      if (stop()) return;
      hash_one(task);
      if (cancelled_latched.load(std::memory_order_acquire)) return;
    }
  };

  const auto requested_workers = hash_workers == 0 ? default_hash_workers : hash_workers;
  const auto worker_count = std::min({requested_workers, maximum_hash_workers,
                                      std::max<std::size_t>(std::size_t{1}, hash_tasks.size())});
  if (worker_count == 1) {
    run_serial();
  } else {
    std::atomic_size_t next_task{0};
    std::atomic_bool halt_workers{false};
    const auto run_worker = [&] {
      while (!halt_workers.load(std::memory_order_acquire)) {
        const auto index = next_task.fetch_add(1, std::memory_order_relaxed);
        if (index >= hash_tasks.size()) return;
        if (stop()) return;
        hash_one(hash_tasks[index]);
      }
    };
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    bool launch_failed = false;
    try {
      for (std::size_t index = 0; index < worker_count; ++index)
        workers.emplace_back(run_worker);
    } catch (...) {
      launch_failed = true;
      halt_workers.store(true, std::memory_order_release);
    }
    for (auto& worker : workers) worker.join();
    if (launch_failed && !stop()) {
      // Thread creation is an implementation detail, not an integrity result.
      // Rehash the complete established list serially rather than certifying a
      // partial set or weakening the verification requirement.
      checks = initial_checks;
      run_serial();
    }
  }
  if (cancelled_latched.load(std::memory_order_acquire)) {
    result.cancelled = true;
    return result;
  }
  // Discovery diagnostics intentionally precede manifest rows, matching the
  // serial verifier's report order.  Hash workers fill only the preallocated
  // manifest portion above.
  for (auto& check : checks)
    result.files.push_back(std::move(check));
  return result;
}

} // namespace off::data
