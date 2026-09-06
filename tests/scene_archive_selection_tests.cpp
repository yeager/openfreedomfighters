#include "off/graphics/scene_render.hpp"

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
  for (unsigned shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
  }
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

void write_zip(
    const std::filesystem::path &path,
    const std::vector<std::pair<std::string, std::string>> &members) {
  struct CentralEntry {
    std::string name;
    std::string payload;
    std::uint32_t crc{};
    std::uint32_t local_offset{};
  };
  std::vector<std::byte> bytes;
  std::vector<CentralEntry> entries;
  for (const auto &[name, payload] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
                static_cast<uInt>(payload.size())));
    const auto local_offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, crc);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0);
    append_text(bytes, name);
    append_text(bytes, payload);
    entries.push_back({name, payload, crc, local_offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : entries) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, static_cast<std::uint32_t>(entry.payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(entry.payload.size()));
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, entry.local_offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    throw std::runtime_error("could not write synthetic ZIP fixture");
  }
}

std::vector<std::pair<std::string, std::string>>
complete_members(bool duplicate_primitive = false) {
  std::vector<std::pair<std::string, std::string>> members{
      {"synthetic/mesh.PRM", "p"},
      {"synthetic/image.TEX", "t"},
      {"synthetic/object.GMS", "g"},
      {"synthetic/map.RMC", "c"},
      {"synthetic/instance.RMI", "i"}};
  if (duplicate_primitive) {
    members.emplace_back("synthetic/duplicate.prm", "d");
  }
  return members;
}

std::string selection_error(const std::filesystem::path &root) {
  try {
    static_cast<void>(off::graphics::load_diagnostic_scene_render_asset(root));
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

  const auto incomplete_root = work / "incomplete";
  std::filesystem::create_directories(incomplete_root / "Scenes");
  write_zip(incomplete_root / "Scenes" / "bootstrap.ZIP",
            {{"synthetic/bootstrap.ZGF", "z"}, {"synthetic/mesh.PRM", "p"}});
  const auto incomplete_error = selection_error(incomplete_root);
  check(incomplete_error ==
            "installation contains no renderable direct-local scene archive",
        "skip archives missing required scene resource families");

  const auto ordering_root = work / "ordering";
  std::filesystem::create_directories(ordering_root / "Scenes");
  write_zip(ordering_root / "Scenes" / "zeta.ZIP", complete_members());
  write_zip(ordering_root / "Scenes" / "ALPHA.zIp", complete_members(true));
  const auto ordering_error = selection_error(ordering_root);
  check(ordering_error ==
            "scene archive contains duplicate scene-resource members",
        "select case-insensitive ZIP candidates in stable path order");
  check(ordering_error.find("ALPHA") == std::string::npos &&
            ordering_error.find(work.string()) == std::string::npos,
        "do not expose scene paths in archive selection errors");

  const auto malformed_root = work / "malformed";
  std::filesystem::create_directories(malformed_root / "Scenes");
  write_zip(malformed_root / "Scenes" / "candidate.ZIP", complete_members());
  const auto malformed_error = selection_error(malformed_root);
  check(!malformed_error.empty() &&
            malformed_error != "installation contains no renderable "
                               "direct-local scene archive" &&
            malformed_error.find("candidate") == std::string::npos,
        "fail closed on a malformed structurally complete archive");

  const auto startup_root = work / "exact-startup";
  std::filesystem::create_directories(startup_root / "Scenes");
  write_zip(startup_root / "Scenes" / "ALPHA.ZIP", complete_members());
  write_zip(startup_root / "Scenes" / "FF-StartUp.ZIP", complete_members(true));
  std::string startup_error;
  try {
    static_cast<void>(off::graphics::load_startup_scene_render_asset(startup_root));
  } catch (const std::exception &failure) {
    startup_error = failure.what();
  }
  check(startup_error == "scene archive contains duplicate scene-resource members" &&
            selection_error(startup_root) != startup_error,
        "exact UI archive loading does not use alphabetical diagnostic selection");

  const auto symlink_root = work / "symlink";
  const auto external_root = work / "external";
  std::filesystem::create_directories(symlink_root / "Scenes");
  std::filesystem::create_directories(external_root);
  const auto external_archive = external_root / "complete.ZIP";
  write_zip(external_archive, complete_members(true));
  std::filesystem::create_symlink(
      external_archive, symlink_root / "Scenes" / "linked.ZIP", error);
  if (!error) {
    check(selection_error(symlink_root) ==
              "installation contains no renderable direct-local scene archive",
          "ignore symlinked scene archive candidates");
  }

  const auto budget_root = work / "budget";
  std::filesystem::create_directories(budget_root / "Scenes");
  for (std::size_t index = 0; index < 1025; ++index) {
    std::ofstream(budget_root / "Scenes" /
                  ("entry-" + std::to_string(index) + ".txt"));
  }
  check(selection_error(budget_root) ==
            "scene archive directory exceeds the safety entry limit",
        "bound enumeration by all directory entries, not only ZIP candidates");

  std::filesystem::remove_all(work, error);
  return failures == 0 ? 0 : 1;
}
