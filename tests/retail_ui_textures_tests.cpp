#include "off/ui/retail_ui_textures.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

off::data::TextureImage image(std::string name, std::uint32_t width,
                              std::uint32_t height, std::byte value = {}) {
  return {
      .encoding = off::data::TextureEncoding::abgr32,
      .width = width,
      .height = height,
      .name = std::move(name),
      .mips = {{.width = width,
                .height = height,
                .encoded = std::vector<std::byte>(
                    static_cast<std::size_t>(width) * height * 4U, value)}}};
}

std::vector<off::data::TextureImage> complete_set() {
  std::vector<off::data::TextureImage> images;
  images.push_back(image("fixture_00", 128, 128));
  images.push_back(image("fixture_01", 128, 128));
  images.push_back(image("fixture_02", 16, 16));
  images.push_back(image("fixture_03", 16, 16));
  for (int ordinal = 1; ordinal <= 8; ++ordinal) {
    images.push_back(image("fixture_" + std::to_string(ordinal + 3), 2, 2,
                           static_cast<std::byte>(ordinal)));
  }
  images.push_back(image("fixture_12", 16, 16));
  images.push_back(image("fixture_13", 16, 16));
  images.push_back(image("fixture_14", 16, 16));
  images.push_back(image("fixture_15", 16, 16));
  return images;
}

std::vector<off::ui::RetailUiTextureBinding> complete_bindings() {
  std::vector<off::ui::RetailUiTextureBinding> bindings;
  for (std::size_t index = 0; index < 16; ++index) {
    bindings.push_back(
        {static_cast<off::ui::RetailUiTextureRole>(index), index});
  }
  return bindings;
}

bool rejects(const std::vector<off::data::TextureImage> &images,
             const std::vector<off::ui::RetailUiTextureBinding> &bindings) {
  try {
    static_cast<void>(off::ui::resolve_retail_ui_textures(images, bindings));
    return false;
  } catch (const std::runtime_error &) {
    return true;
  }
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

void write_zip(const std::filesystem::path &path,
               const std::vector<std::string> &members) {
  struct CentralEntry {
    std::string name;
    std::uint32_t crc{};
    std::uint32_t offset{};
  };
  std::vector<std::byte> bytes;
  std::vector<CentralEntry> entries;
  for (const auto &name : members) {
    constexpr std::string_view payload{"synthetic"};
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
                static_cast<uInt>(payload.size())));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20); append_u16(bytes, 0); append_u16(bytes, 0);
    append_u16(bytes, 0); append_u16(bytes, 0); append_u32(bytes, crc);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u16(bytes, static_cast<std::uint16_t>(name.size())); append_u16(bytes, 0);
    append_text(bytes, name); append_text(bytes, payload);
    entries.push_back({name, crc, offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : entries) {
    constexpr std::string_view payload{"synthetic"};
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20); append_u16(bytes, 20); append_u16(bytes, 0);
    append_u16(bytes, 0); append_u16(bytes, 0); append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0); append_u16(bytes, 0); append_u16(bytes, 0);
    append_u16(bytes, 0); append_u32(bytes, 0); append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size = static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0); append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u16(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u32(bytes, central_size); append_u32(bytes, central_offset); append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write synthetic ZIP fixture");
}

std::vector<std::string> startup_members() {
  return {"SCENES/FF-StartUp.ZGF", "SCENES/FF-StartUp.SUP",
          "SCENES/FF-StartUp.BUF", "SCENES/FF-StartUp.GMS",
          "SCENES/FF-StartUp.TEX", "SCENES/FF-StartUp.SND",
          "SCENES/FF-StartUp.LOC", "SCENES/FF-StartUp.OCT",
          "SCENES/FF-StartUp.SGP", "SCENES/FF-StartUp.RMC",
          "SCENES/FF-StartUp.RMI"};
}

std::string load_error(const std::filesystem::path &archive) {
  try {
    static_cast<void>(off::ui::load_retail_ui_textures(archive, {}));
  } catch (const std::runtime_error &error) {
    return error.what();
  }
  return {};
}

} // namespace

int main() {
  auto images = complete_set();
  auto bindings = complete_bindings();
  const auto textures = off::ui::resolve_retail_ui_textures(images, bindings);
  check(textures.textures().size() == 16, "resolve every required role");
  check(textures.find(off::ui::RetailUiTextureRole::arrow_left) != nullptr,
        "look up a resolved role without exposing a retail identifier");

  auto reversed = bindings;
  std::ranges::reverse(reversed);
  const auto resolved_reversed =
      off::ui::resolve_retail_ui_textures(images, reversed);
  check(resolved_reversed.textures().size() == bindings.size() &&
            resolved_reversed.textures().front().role ==
                off::ui::RetailUiTextureRole::scanlines_top &&
            resolved_reversed.textures().back().role ==
                off::ui::RetailUiTextureRole::arrow_down,
        "canonicalize output independently of binding order");

  auto duplicate_names = images;
  for (auto &candidate : duplicate_names)
    candidate.name = "duplicate_fixture_name";
  const auto resolved_duplicate_names =
      off::ui::resolve_retail_ui_textures(duplicate_names, bindings);
  check(resolved_duplicate_names.textures().size() == 16,
        "resolve explicit bindings independently of duplicate image names");

  auto shared_fill = bindings;
  shared_fill[3].image_index = shared_fill[2].image_index;
  check(rejects(images, shared_fill),
        "reject reuse between distinct top and bottom fill roles");

  auto missing = bindings;
  missing.pop_back();
  check(rejects(images, missing), "reject a partial role set");

  auto ambiguous = bindings;
  ambiguous.back().role = ambiguous.front().role;
  check(rejects(images, ambiguous), "reject an ambiguous exclusive role");

  auto reused = bindings;
  reused.back().image_index = reused[reused.size() - 2].image_index;
  check(rejects(images, reused), "reject reuse between exclusive roles");

  auto wrong_size = images;
  wrong_size[0] = image("fixture_wrong_size", 64, 64);
  check(rejects(wrong_size, bindings), "enforce recovered role dimensions");

  auto out_of_range_role = bindings;
  out_of_range_role[0].role = static_cast<off::ui::RetailUiTextureRole>(255);
  check(rejects(images, out_of_range_role), "reject an unknown semantic role");

  auto out_of_range_image = bindings;
  out_of_range_image[0].image_index = images.size();
  check(rejects(images, out_of_range_image),
        "reject an out-of-range image binding");

  auto empty_mips = images;
  empty_mips[0].mips.clear();
  check(rejects(empty_mips, bindings), "reject an image without mip zero");

  auto mismatched_mip = images;
  mismatched_mip[0].mips.front().width = 64;
  check(rejects(mismatched_mip, bindings),
        "reject disagreement between image and mip dimensions");

  auto malformed_mip = images;
  malformed_mip[0].mips.front().encoded.pop_back();
  check(rejects(malformed_mip, bindings),
        "reject a truncated encoded retail UI mip");

  const std::filesystem::path work = OFF_TEST_WORK_DIR;
  std::error_code filesystem_error;
  std::filesystem::remove_all(work, filesystem_error);
  std::filesystem::create_directories(work);

  auto canonical = startup_members();
  const auto exact = work / "exact.zip";
  write_zip(exact, canonical);
  const auto exact_error = load_error(exact);
  check(!exact_error.empty() &&
            exact_error != "retail UI texture archive is not the canonical startup family",
        "admit only the exact startup member family before TEX parsing");

  auto missing_member = canonical;
  missing_member.erase(
      std::ranges::find(missing_member, "SCENES/FF-StartUp.GMS"));
  const auto missing_archive = work / "missing.zip";
  write_zip(missing_archive, missing_member);
  check(load_error(missing_archive) ==
            "retail UI texture archive is not the canonical startup family",
        "reject a startup archive missing a required sibling member");

  auto extra = canonical;
  extra.push_back("SCENES/Other.TEX");
  const auto extra_archive = work / "extra.zip";
  write_zip(extra_archive, extra);
  check(load_error(extra_archive) ==
            "retail UI texture archive is not the canonical startup family",
        "reject an archive with an unrelated texture member");

  auto wrong_case = canonical;
  *std::ranges::find(wrong_case, "SCENES/FF-StartUp.TEX") =
      "SCENES/ff-startup.TEX";
  const auto wrong_case_archive = work / "wrong-case.zip";
  write_zip(wrong_case_archive, wrong_case);
  check(load_error(wrong_case_archive) ==
            "retail UI texture archive is not the canonical startup family",
        "require the recovered startup member spelling rather than ZIP lookup normalization");

  std::filesystem::remove_all(work, filesystem_error);
}
