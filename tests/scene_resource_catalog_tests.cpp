#include "off/data/scene_resource_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32U; shift += 8U)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

void write_zip(const std::filesystem::path &path,
               const std::vector<std::string> &members) {
  struct Central {
    std::string name;
    std::uint32_t crc;
    std::uint32_t offset;
  };
  std::vector<std::byte> bytes;
  std::vector<Central> central;
  for (const auto &name : members) {
    constexpr std::string_view payload{"x"};
    const auto crc = static_cast<std::uint32_t>(::crc32(
        0, reinterpret_cast<const Bytef *>(payload.data()), payload.size()));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u32(bytes, crc);
    append_u32(bytes, 1U);
    append_u32(bytes, 1U);
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0U);
    append_text(bytes, name);
    append_text(bytes, payload);
    central.push_back({name, crc, offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : central) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20U);
    append_u16(bytes, 20U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u32(bytes, entry.crc);
    append_u32(bytes, 1U);
    append_u32(bytes, 1U);
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u16(bytes, 0U);
    append_u32(bytes, 0U);
    append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0U);
  append_u16(bytes, 0U);
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0U);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::string> complete_members() {
  std::vector<std::string> result;
  for (const auto extension :
       {"ZGF", "SUP", "GMS", "TEX", "PRM", "RMC", "RMI", "SND", "OCT", "SGP"})
    result.emplace_back("scene/bootstrap." + std::string(extension));
  return result;
}

std::string catalog_error(const std::filesystem::path &path) {
  try {
    static_cast<void>(off::data::SceneResourceCatalog::from_archive(
        off::data::ZipArchive::open(path)));
  } catch (const std::exception &error) {
    return error.what();
  }
  return {};
}

} // namespace

int main() {
  const std::filesystem::path work = OFF_TEST_WORK_DIR;
  std::error_code error;
  std::filesystem::remove_all(work, error);
  std::filesystem::create_directories(work);

  auto complete = complete_members();
  complete.emplace_back("scene/bootstrap.BUF");
  complete.emplace_back("scene/bootstrap.LOC");
  complete.emplace_back("scene/bootstrap.ANM");
  write_zip(work / "complete.zip", complete);
  const auto archive = off::data::ZipArchive::open(work / "complete.zip");
  const auto catalog = off::data::SceneResourceCatalog::from_archive(archive);
  check(catalog.required_members().size() == 10U,
        "catalog retains every required member by role");
  check(catalog.member(off::data::SceneResourceKind::tex).name ==
                "scene/bootstrap.TEX" &&
            catalog.optional_member(off::data::SceneResourceKind::anm) !=
                nullptr,
        "catalog preserves typed required and optional ownership");
  check(catalog.optional_member(off::data::SceneResourceKind::tex) == nullptr,
        "required members cannot be retrieved as optional");

  auto mixed_stem = complete_members();
  mixed_stem.back() = "scene/other.SGP";
  write_zip(work / "mixed-stem.zip", mixed_stem);
  check(catalog_error(work / "mixed-stem.zip") ==
            "scene archive resources do not share one source identity",
        "reject a mixed scene resource stem before payload parsing");

  auto mixed_directory = complete_members();
  mixed_directory.back() = "other/bootstrap.SGP";
  write_zip(work / "mixed-directory.zip", mixed_directory);
  check(catalog_error(work / "mixed-directory.zip") ==
            "scene archive resources do not share one source identity",
        "reject a mixed scene resource directory before payload parsing");

  auto duplicate = complete_members();
  duplicate.emplace_back("scene/other.PRM");
  write_zip(work / "duplicate.zip", duplicate);
  check(catalog_error(work / "duplicate.zip") ==
            "scene archive contains duplicate scene-resource members",
        "reject duplicate typed scene resources");

  std::filesystem::remove_all(work, error);
  return failures == 0 ? 0 : 1;
}
