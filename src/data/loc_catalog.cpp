#include "off/data/loc_catalog.hpp"

#include "off/crypto/sha256.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

namespace off::data {
namespace {

constexpr std::size_t maximum_member_bytes = 256U * 1024U * 1024U;
constexpr std::size_t maximum_nodes = 1'000'000U;
constexpr std::size_t maximum_values = 1'000'000U;
constexpr std::size_t maximum_value_bytes = 1U * 1024U * 1024U;
constexpr std::size_t maximum_catalog_bytes = 128U * 1024U * 1024U;
constexpr std::size_t expected_member_count = 88U;

[[nodiscard]] bool valid_utf8(std::string_view value) noexcept {
  for (std::size_t cursor{}; cursor < value.size();) {
    const auto lead = static_cast<unsigned char>(value[cursor]);
    if (lead <= 0x7fU) { ++cursor; continue; }
    const auto length = lead >= 0xc2U && lead <= 0xdfU ? 2U :
                        lead >= 0xe0U && lead <= 0xefU ? 3U :
                        lead >= 0xf0U && lead <= 0xf4U ? 4U : 0U;
    if (length == 0U || length > value.size() - cursor) return false;
    std::uint32_t code_point = lead & (length == 2U ? 0x1fU : length == 3U ? 0x0fU : 0x07U);
    for (unsigned index{1U}; index < length; ++index) {
      const auto byte = static_cast<unsigned char>(value[cursor + index]);
      if ((byte & 0xc0U) != 0x80U) return false;
      code_point = (code_point << 6U) | (byte & 0x3fU);
    }
    const auto minimum = length == 2U ? 0x80U : length == 3U ? 0x800U : 0x10000U;
    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) return false;
    cursor += length;
  }
  return true;
}

[[nodiscard]] std::string folded(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte > 0x7fU) return {};
    const auto slash = character == '\\' ? '/' : character;
    result.push_back(static_cast<char>(slash >= 'A' && slash <= 'Z' ? slash + ('a' - 'A') : slash));
  }
  return result;
}

[[nodiscard]] bool has_zip_extension(const std::filesystem::path& path) {
  return folded(path.extension().string()) == ".zip";
}

[[nodiscard]] bool has_loc_extension(std::string_view value) {
  const auto dot = value.find_last_of('.');
  return dot != std::string_view::npos && folded(value.substr(dot)) == ".loc";
}

[[nodiscard]] std::string bytes_sha256(std::span<const std::byte> bytes) {
  crypto::Sha256 digest;
  digest.update(bytes);
  return crypto::to_hex(digest.finish());
}

[[nodiscard]] std::optional<std::uint32_t> read_u32(std::span<const std::byte> bytes,
                                                      std::size_t offset,
                                                      std::size_t end) noexcept {
  if (offset > end || end - offset < sizeof(std::uint32_t)) return std::nullopt;
  std::uint32_t result{};
  for (unsigned index{}; index < sizeof(result); ++index) {
    result |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + index])) << (index * 8U);
  }
  return result;
}

class Decoder final {
public:
  explicit Decoder(std::span<const std::byte> bytes) : bytes_(bytes) {}

  [[nodiscard]] std::optional<LocMemberDisplayText> decode() {
    if (bytes_.empty() || bytes_.size() > maximum_member_bytes || !node(0U, bytes_.size(), 0U, false))
      return std::nullopt;
    return LocMemberDisplayText{std::move(values_)};
  }

private:
  [[nodiscard]] std::optional<std::pair<std::size_t, std::string_view>> c_string(
      std::size_t cursor, std::size_t end) const noexcept {
    if (cursor >= end) return std::nullopt;
    auto terminal = cursor;
    while (terminal < end && bytes_[terminal] != std::byte{}) ++terminal;
    if (terminal == end) return std::nullopt;
    const auto* first = reinterpret_cast<const char*>(bytes_.data() + cursor);
    const std::string_view value{first, terminal - cursor};
    if (!valid_utf8(value)) return std::nullopt;
    return std::pair{terminal + 1U, value};
  }

  [[nodiscard]] bool append_value(std::string_view value) {
    if (value.size() > maximum_value_bytes || values_.size() == maximum_values ||
        value.size() > maximum_catalog_bytes - total_value_bytes_) return false;
    total_value_bytes_ += value.size();
    values_.emplace_back(value);
    return true;
  }

  [[nodiscard]] bool node(std::size_t cursor, std::size_t end, std::size_t depth,
                          bool named) {
    if (cursor >= end || depth > 64U) return false;
    if (named) {
      const auto key = c_string(cursor, end);
      if (!key || key->second.empty() || ++nodes_ > maximum_nodes) return false;
      cursor = key->first;
      if (cursor == end) return false;
    }
    while (cursor < end) {
      const auto marker = static_cast<unsigned char>(bytes_[cursor++]);
      if (marker == 0U) continue;
      if (marker == 1U) {
        const auto value = c_string(cursor, end);
        if (!value || !append_value(value->second)) return false;
        cursor = value->first;
        continue;
      }
      const auto offset_count = static_cast<std::size_t>(marker - 1U);
      if (offset_count > (std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t))) return false;
      const auto table_bytes = offset_count * sizeof(std::uint32_t);
      bool list = cursor <= end && table_bytes <= end - cursor;
      std::vector<std::size_t> offsets;
      std::size_t payload{};
      if (list) {
        payload = cursor + table_bytes;
        const auto remaining = end - payload;
        offsets.reserve(offset_count);
        std::size_t previous{};
        for (std::size_t index{}; index < offset_count; ++index) {
          const auto raw = read_u32(bytes_, cursor + index * sizeof(std::uint32_t), end);
          if (!raw || *raw == 0U || *raw >= remaining || (index != 0U && *raw <= previous)) {
            list = false;
            break;
          }
          previous = *raw;
          offsets.push_back(*raw);
        }
      }
      if (list) {
        std::size_t start{};
        for (const auto boundary : offsets) {
          if (!node(payload + start, payload + boundary, depth + 1U, true)) return false;
          start = boundary;
        }
        if (!node(payload + start, end, depth + 1U, true)) return false;
        return true;
      }
      // A small, observed terminal form encodes marker-1 sequential values
      // instead of a child-offset table.  It is admitted only when those
      // complete the exact node remainder; there is no heuristic fallback.
      auto value_cursor = cursor;
      bool complete = true;
      std::vector<std::string_view> values;
      values.reserve(offset_count);
      for (std::size_t index{}; index < offset_count; ++index) {
        const auto value = c_string(value_cursor, end);
        if (!value) { complete = false; break; }
        values.push_back(value->second);
        value_cursor = value->first;
      }
      if (complete && value_cursor == end) {
        for (const auto value : values) if (!append_value(value)) return false;
        return true;
      }
      // One verified marker-only terminal has no source value.  It is not
      // promoted to display text, and any bytes after it remain an error.
      if (cursor == end) return true;
      return false;
    }
    return true;
  }

  std::span<const std::byte> bytes_;
  std::vector<std::string> values_;
  std::size_t nodes_{};
  std::size_t total_value_bytes_{};
};

struct Member final {
  std::string id;
  // The digest binds a source-set to the complete decoded LOC member without
  // retaining or emitting any of its text.
  std::string content_sha256;
  std::filesystem::path archive;
};

[[nodiscard]] std::optional<std::vector<Member>> discover_members(
    const std::filesystem::path& root) {
  std::error_code error;
  const auto scenes = root / "Scenes";
  const auto scenes_status = std::filesystem::symlink_status(scenes, error);
  if (error || !std::filesystem::is_directory(scenes_status) || std::filesystem::is_symlink(scenes_status))
    return std::nullopt;
  std::vector<std::filesystem::path> archives;
  for (std::filesystem::recursive_directory_iterator iterator(scenes, error), end;
       !error && iterator != end; iterator.increment(error)) {
    const auto status = iterator->symlink_status(error);
    if (error || std::filesystem::is_symlink(status)) return std::nullopt;
    if (std::filesystem::is_regular_file(status) && has_zip_extension(iterator->path()))
      archives.push_back(iterator->path());
  }
  if (error || archives.size() > 256U) return std::nullopt;
  std::ranges::sort(archives, {}, [](const auto& path) { return folded(path.generic_string()); });

  std::vector<Member> members;
  for (const auto& archive_path : archives) {
    const auto relative = archive_path.lexically_relative(root).generic_string();
    const auto archive_id = folded(relative);
    if (archive_id.empty()) return std::nullopt;
    ZipArchive archive;
    try { archive = ZipArchive::open(archive_path); }
    catch (const std::exception&) { return std::nullopt; }
    std::vector<const ZipEntry*> entries;
    for (const auto& entry : archive.entries()) if (has_loc_extension(entry.name)) entries.push_back(&entry);
    if (entries.size() > 1U) return std::nullopt;
    if (entries.empty()) continue;
    const auto entry_id = folded(entries.front()->name);
    if (entry_id.empty()) return std::nullopt;
    std::vector<std::byte> bytes;
    try { bytes = archive.read(*entries.front()); }
    catch (const std::exception&) { return std::nullopt; }
    if (bytes.size() > maximum_member_bytes) return std::nullopt;
    members.push_back({archive_id + "|" + entry_id,
                       bytes_sha256(bytes),
                       archive_path});
  }
  if (members.size() != expected_member_count) return std::nullopt;
  std::ranges::sort(members, {}, &Member::id);
  if (std::ranges::adjacent_find(members, {}, &Member::id) != members.end()) return std::nullopt;
  return members;
}

[[nodiscard]] std::string source_set_for(std::span<const Member> members) {
  std::string identity_input{loc_catalog_parser_identity};
  for (const auto& member : members) {
    identity_input.push_back('\n');
    identity_input += member.id;
    identity_input.push_back(':');
    identity_input += member.content_sha256;
  }
  return "loc-source-" + crypto::to_hex(crypto::sha256(identity_input));
}

}  // namespace

std::optional<LocMemberDisplayText>
decode_loc_member_display_texts(std::span<const std::byte> bytes) {
  return Decoder{bytes}.decode();
}

std::optional<OwnedLocCatalog>
extract_verified_owned_loc_catalog(const std::filesystem::path& root) {
  const auto members = discover_members(root);
  if (!members) return std::nullopt;
  OwnedLocCatalog result;
  for (const auto& member : *members) {
    ZipArchive archive;
    try { archive = ZipArchive::open(member.archive); }
    catch (const std::exception&) { return std::nullopt; }
    const auto separator = member.id.find('|');
    if (separator == std::string::npos) return std::nullopt;
    const auto wanted = member.id.substr(separator + 1U);
    const auto found = std::find_if(archive.entries().begin(), archive.entries().end(), [&](const ZipEntry& entry) {
      return folded(entry.name) == wanted;
    });
    if (found == archive.entries().end()) return std::nullopt;
    std::vector<std::byte> bytes;
    try { bytes = archive.read(*found); }
    catch (const std::exception&) { return std::nullopt; }
    const auto decoded = decode_loc_member_display_texts(bytes);
    if (!decoded || decoded->values.size() > maximum_values - result.values.size()) return std::nullopt;
    std::size_t addition{};
    for (const auto& value : decoded->values) {
      if (value.size() > maximum_catalog_bytes - addition) return std::nullopt;
      addition += value.size();
    }
    std::size_t existing{};
    for (const auto& value : result.values) {
      if (value.size() > maximum_catalog_bytes - existing) return std::nullopt;
      existing += value.size();
    }
    if (addition > maximum_catalog_bytes - existing) return std::nullopt;
    result.values.insert(result.values.end(), decoded->values.begin(), decoded->values.end());
  }
  if (result.values.empty()) return std::nullopt;
  result.source_set = source_set_for(*members);
  return result;
}

std::optional<std::string>
verified_owned_loc_source_set(const std::filesystem::path& root) {
  const auto members = discover_members(root);
  if (!members) return std::nullopt;
  return source_set_for(*members);
}

}  // namespace off::data
