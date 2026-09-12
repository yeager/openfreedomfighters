#include "off/simulation/save_store.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace off::simulation {
namespace {

constexpr std::array<std::uint8_t, 8> magic{'O', 'F', 'F', 'S', 'A', 'V', 'E', 0};
constexpr std::uint32_t schema_version = 1;
constexpr std::uint32_t byte_order_marker = 0x01020304U;
constexpr std::size_t header_size = 68;
constexpr std::size_t maximum_document_bytes = 64U * 1024U * 1024U;
constexpr std::size_t maximum_campaign_bytes = 128;
constexpr unsigned temporary_attempts = 32;
std::atomic<unsigned long> temporary_counter{0};

class Writer final {
public:
  template <class Integer> void integer(Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto encoded = static_cast<Unsigned>(value);
    for (std::size_t i = 0; i < sizeof(encoded); ++i) {
      bytes_.push_back(static_cast<std::byte>(encoded & 0xffU));
      if constexpr (sizeof(Unsigned) > 1)
        encoded >>= 8U;
    }
  }
  void bytes(std::span<const std::byte> value) {
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }
  [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }
private:
  std::vector<std::byte> bytes_;
};

class Reader final {
public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  template <class Integer> [[nodiscard]] Integer integer() {
    using Unsigned = std::make_unsigned_t<Integer>;
    if (position_ > bytes_.size() || bytes_.size() - position_ < sizeof(Unsigned))
      throw std::invalid_argument("truncated project save");
    std::uint64_t result{};
    for (std::size_t i = 0; i < sizeof(Unsigned); ++i)
      result |= static_cast<std::uint64_t>(
                    std::to_integer<std::uint8_t>(bytes_[position_++]))
                << (i * 8U);
    return static_cast<Integer>(result);
  }
  [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) {
    if (position_ > bytes_.size() || count > bytes_.size() - position_)
      throw std::invalid_argument("truncated project save");
    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
  }
  [[nodiscard]] bool exhausted() const noexcept { return position_ == bytes_.size(); }
private:
  std::span<const std::byte> bytes_;
  std::size_t position_{};
};

[[nodiscard]] bool valid_campaign_id(std::string_view value) noexcept {
  if (value.empty() || value.size() > maximum_campaign_bytes)
    return false;
  for (const auto character : value) {
    const auto c = static_cast<unsigned char>(character);
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
      return false;
  }
  return true;
}

[[nodiscard]] bool valid_identity(const ProjectSaveIdentity &identity) noexcept {
  return valid_campaign_id(identity.campaign_id);
}

struct DecodedSave final {
  std::uint64_t generation{};
  std::vector<std::byte> world_snapshot;
};

[[nodiscard]] std::vector<std::byte>
encode_save(const SimulationWorld &world, const ProjectSaveIdentity &identity,
            std::uint64_t generation) {
  if (!valid_identity(identity) || generation == 0)
    throw std::invalid_argument("project save identity or generation is invalid");
  const auto snapshot = world.export_snapshot();
  Writer payload;
  payload.integer<std::uint16_t>(static_cast<std::uint16_t>(identity.campaign_id.size()));
  payload.bytes({reinterpret_cast<const std::byte *>(identity.campaign_id.data()), identity.campaign_id.size()});
  payload.bytes({reinterpret_cast<const std::byte *>(identity.data_manifest_fingerprint.data()), identity.data_manifest_fingerprint.size()});
  payload.bytes(snapshot);
  auto body = std::move(payload).take();
  crypto::Sha256 body_hasher;
  body_hasher.update(body);
  const auto checksum = body_hasher.finish();
  Writer document;
  for (const auto byte : magic) document.integer<std::uint8_t>(byte);
  document.integer<std::uint32_t>(schema_version);
  document.integer<std::uint32_t>(byte_order_marker);
  document.integer<std::uint32_t>(header_size);
  document.integer<std::uint64_t>(generation);
  document.integer<std::uint64_t>(static_cast<std::uint64_t>(body.size()));
  document.bytes({reinterpret_cast<const std::byte *>(checksum.data()), checksum.size()});
  document.bytes(body);
  return std::move(document).take();
}

[[nodiscard]] DecodedSave decode_save(std::span<const std::byte> document,
                                      const ProjectSaveIdentity &identity) {
  if (!valid_identity(identity) || document.size() < header_size ||
      document.size() > maximum_document_bytes)
    throw std::invalid_argument("project save size or identity is invalid");
  Reader header(document.first(header_size));
  for (const auto expected : magic)
    if (header.integer<std::uint8_t>() != expected)
      throw std::invalid_argument("project save magic is invalid");
  if (header.integer<std::uint32_t>() != schema_version ||
      header.integer<std::uint32_t>() != byte_order_marker ||
      header.integer<std::uint32_t>() != header_size)
    throw std::invalid_argument("project save version is unsupported");
  const auto generation = header.integer<std::uint64_t>();
  const auto payload_size = header.integer<std::uint64_t>();
  const auto expected_checksum = header.bytes(32);
  if (!header.exhausted() || generation == 0 ||
      payload_size != document.size() - header_size)
    throw std::invalid_argument("project save header is invalid");
  const auto body = document.subspan(header_size);
  crypto::Sha256 body_hasher;
  body_hasher.update(body);
  const auto actual_checksum = body_hasher.finish();
  if (!std::ranges::equal(expected_checksum,
                          std::span<const std::byte>{reinterpret_cast<const std::byte *>(actual_checksum.data()), actual_checksum.size()}))
    throw std::invalid_argument("project save checksum is invalid");
  Reader reader(body);
  const auto campaign_size = reader.integer<std::uint16_t>();
  if (campaign_size == 0 || campaign_size > maximum_campaign_bytes)
    throw std::invalid_argument("project save campaign is invalid");
  const auto campaign_bytes = reader.bytes(campaign_size);
  const std::string campaign(reinterpret_cast<const char *>(campaign_bytes.data()), campaign_bytes.size());
  if (campaign != identity.campaign_id)
    throw std::invalid_argument("project save campaign does not match");
  const auto fingerprint = reader.bytes(identity.data_manifest_fingerprint.size());
  if (!std::ranges::equal(fingerprint,
                          std::span<const std::byte>{reinterpret_cast<const std::byte *>(identity.data_manifest_fingerprint.data()), identity.data_manifest_fingerprint.size()}))
    throw std::invalid_argument("project save data manifest does not match");
  const auto snapshot = reader.bytes(body.size() - sizeof(std::uint16_t) -
                                     campaign_size - identity.data_manifest_fingerprint.size());
  if (!reader.exhausted())
    throw std::invalid_argument("project save has trailing payload bytes");
  SimulationWorld staged;
  staged.import_snapshot(snapshot);
  return {generation, {snapshot.begin(), snapshot.end()}};
}

enum class FileReadStatus : unsigned char { missing, decoded, invalid, io_error };
struct FileReadResult final { FileReadStatus status{FileReadStatus::missing}; std::optional<DecodedSave> decoded; };

[[nodiscard]] bool regular_file_or_missing(const std::filesystem::path &path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      status.type() == std::filesystem::file_type::not_found)
    return true;
  return !error && std::filesystem::is_regular_file(status);
}

enum class DocumentReadStatus : unsigned char { read, missing, invalid, io_error };

[[nodiscard]] DocumentReadStatus read_document_no_follow(
    const std::filesystem::path &path, std::vector<std::byte> &bytes) {
#ifdef _WIN32
  const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                                nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const auto error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
               ? DocumentReadStatus::missing
               : DocumentReadStatus::io_error;
  }
  BY_HANDLE_FILE_INFORMATION information{};
  LARGE_INTEGER size{};
  const bool valid_file = GetFileInformationByHandle(file, &information) != 0 &&
                          GetFileSizeEx(file, &size) != 0 &&
                          (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
                          size.QuadPart >= 0 &&
                          static_cast<std::uint64_t>(size.QuadPart) <= maximum_document_bytes;
  if (!valid_file) { CloseHandle(file); return DocumentReadStatus::invalid; }
  bytes.resize(static_cast<std::size_t>(size.QuadPart));
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto remaining = bytes.size() - offset;
    const auto chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD));
    DWORD read{};
    if (ReadFile(file, bytes.data() + offset, chunk, &read, nullptr) == 0 || read == 0) {
      CloseHandle(file); return DocumentReadStatus::io_error;
    }
    offset += read;
  }
  CloseHandle(file);
  return DocumentReadStatus::read;
#else
  const int file = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
  if (file < 0) {
    if (errno == ENOENT) return DocumentReadStatus::missing;
    if (errno == ELOOP) return DocumentReadStatus::invalid;
    return DocumentReadStatus::io_error;
  }
  struct stat information {};
  if (fstat(file, &information) != 0 || !S_ISREG(information.st_mode) ||
      information.st_size < 0 ||
      static_cast<std::uintmax_t>(information.st_size) > maximum_document_bytes) {
    close(file); return DocumentReadStatus::invalid;
  }
  bytes.resize(static_cast<std::size_t>(information.st_size));
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto read_count = read(file, bytes.data() + offset, bytes.size() - offset);
    if (read_count < 0) { if (errno == EINTR) continue; close(file); return DocumentReadStatus::io_error; }
    if (read_count == 0) { close(file); return DocumentReadStatus::invalid; }
    offset += static_cast<std::size_t>(read_count);
  }
  close(file);
  return DocumentReadStatus::read;
#endif
}

[[nodiscard]] FileReadResult read_save(const std::filesystem::path &path,
                                       const ProjectSaveIdentity &identity) {
  std::vector<std::byte> bytes;
  switch (read_document_no_follow(path, bytes)) {
  case DocumentReadStatus::missing: return {};
  case DocumentReadStatus::invalid: return {FileReadStatus::invalid, {}};
  case DocumentReadStatus::io_error: return {FileReadStatus::io_error, {}};
  case DocumentReadStatus::read: break;
  }
  try { return {FileReadStatus::decoded, decode_save(bytes, identity)}; }
  catch (const std::exception &) { return {FileReadStatus::invalid, {}}; }
}

#ifndef _WIN32
[[nodiscard]] bool write_all(int file, std::span<const std::byte> bytes) {
  while (!bytes.empty()) {
    const auto written = write(file, bytes.data(), bytes.size());
    if (written < 0) { if (errno == EINTR) continue; return false; }
    if (written == 0) return false;
    bytes = bytes.subspan(static_cast<std::size_t>(written));
  }
  return true;
}
#endif

[[nodiscard]] bool write_replace(const std::filesystem::path &destination,
                                 std::span<const std::byte> bytes) {
  if (destination.empty() || !regular_file_or_missing(destination)) return false;
  const auto directory = destination.parent_path().empty() ? std::filesystem::path{"."} : destination.parent_path();
  for (unsigned attempt = 0; attempt < temporary_attempts; ++attempt) {
    const auto temporary = directory / (destination.filename().string() + ".tmp-" + std::to_string(temporary_counter.fetch_add(1)));
#ifdef _WIN32
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE) continue;
    DWORD written{};
    const bool written_ok = bytes.size() <= MAXDWORD &&
        WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0 &&
        written == bytes.size() && FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (!written_ok) { std::error_code ignored; std::filesystem::remove(temporary, ignored); return false; }
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) return true;
#else
    const int file = open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    if (file < 0) { if (errno == EEXIST) continue; return false; }
    const bool written_ok = write_all(file, bytes) && fsync(file) == 0;
    close(file);
    if (!written_ok) { std::error_code ignored; std::filesystem::remove(temporary, ignored); return false; }
    std::error_code rename_error;
    std::filesystem::rename(temporary, destination, rename_error);
    if (!rename_error) {
      const int directory_file = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
      if (directory_file < 0) return false;
      const bool synced = fsync(directory_file) == 0;
      close(directory_file);
      return synced;
    }
#endif
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  return false;
}

} // namespace

ProjectSaveStore::ProjectSaveStore(std::filesystem::path slot_root) : slot_root_(std::move(slot_root)) {}

std::filesystem::path ProjectSaveStore::generation_path(unsigned generation) const {
  if (generation > 1 || slot_root_.empty()) return {};
  return slot_root_.parent_path() /
         (slot_root_.filename().string() + "." + std::to_string(generation) + ".offsave");
}

SaveLoadResult ProjectSaveStore::load(SimulationWorld &world,
                                      const ProjectSaveIdentity &identity) const {
  const auto first = read_save(generation_path(0), identity);
  const auto second = read_save(generation_path(1), identity);
  if (first.status == FileReadStatus::io_error || second.status == FileReadStatus::io_error)
    return {SaveLoadStatus::io_error, 0};
  const auto valid_count = static_cast<unsigned>(first.status == FileReadStatus::decoded) +
                           static_cast<unsigned>(second.status == FileReadStatus::decoded);
  if (!valid_count) {
    if (first.status == FileReadStatus::missing && second.status == FileReadStatus::missing)
      return {SaveLoadStatus::missing, 0};
    return {SaveLoadStatus::invalid, 0};
  }
  const auto *selected = first.status == FileReadStatus::decoded ? &*first.decoded : &*second.decoded;
  if (valid_count == 2) {
    if (first.decoded->generation == second.decoded->generation)
      return {SaveLoadStatus::ambiguous, 0};
    selected = first.decoded->generation > second.decoded->generation ? &*first.decoded : &*second.decoded;
  }
  try {
    SimulationWorld staged;
    staged.import_snapshot(selected->world_snapshot);
    world = std::move(staged);
    return {SaveLoadStatus::loaded, selected->generation};
  } catch (const std::exception &) {
    return {SaveLoadStatus::invalid, 0};
  }
}

bool ProjectSaveStore::save(const SimulationWorld &world,
                            const ProjectSaveIdentity &identity) const {
  if (!valid_identity(identity) || generation_path(0).empty() || generation_path(1).empty())
    return false;
  const auto first = read_save(generation_path(0), identity);
  const auto second = read_save(generation_path(1), identity);
  if (first.status == FileReadStatus::io_error || second.status == FileReadStatus::io_error)
    return false;
  if (first.status == FileReadStatus::decoded && second.status == FileReadStatus::decoded &&
      first.decoded->generation == second.decoded->generation)
    return false;
  std::uint64_t newest{};
  if (first.status == FileReadStatus::decoded) newest = first.decoded->generation;
  if (second.status == FileReadStatus::decoded) newest = std::max(newest, second.decoded->generation);
  if (newest == std::numeric_limits<std::uint64_t>::max()) return false;
  const unsigned target = first.status != FileReadStatus::decoded ? 0 :
                          second.status != FileReadStatus::decoded ? 1 :
                          first.decoded->generation < second.decoded->generation ? 0 : 1;
  try { return write_replace(generation_path(target), encode_save(world, identity, newest + 1)); }
  catch (const std::exception &) { return false; }
}

} // namespace off::simulation
