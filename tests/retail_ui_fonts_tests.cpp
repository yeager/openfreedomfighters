#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/font_run_layout.hpp"

#include <algorithm>
#include <array>
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

constexpr std::uint32_t zgf_magic = 0x5a474654U;
constexpr std::uint32_t root_flag = 0x80000000U;
int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void append_u16_le(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32_le(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_u16_be(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value >> 8U));
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
}

void append_u32_be(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void set_u32_le(std::vector<std::byte> &bytes, std::size_t offset,
                std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

std::vector<std::byte> valid_sfnt(std::uint32_t signature = 0x00010000U) {
  std::vector<std::byte> bytes;
  append_u32_be(bytes, signature);
  append_u16_be(bytes, 1);
  append_u16_be(bytes, 16);
  append_u16_be(bytes, 0);
  append_u16_be(bytes, 0);
  append_text(bytes, "head");
  append_u32_be(bytes, 0);
  append_u32_be(bytes, 28);
  append_u32_be(bytes, 4);
  append_u32_be(bytes, 0x12345678U);
  return bytes;
}

std::vector<std::byte> cmap12_sfnt(std::uint32_t first, std::uint32_t last) {
  auto bytes = valid_sfnt();
  bytes[12] = std::byte{'c'}; bytes[13] = std::byte{'m'};
  bytes[14] = std::byte{'a'}; bytes[15] = std::byte{'p'};
  bytes[24] = std::byte{0}; bytes[25] = std::byte{0};
  bytes[26] = std::byte{0}; bytes[27] = std::byte{40};
  bytes.resize(68);
  bytes[28] = std::byte{0}; bytes[29] = std::byte{0}; bytes[30] = std::byte{0}; bytes[31] = std::byte{1};
  bytes[32] = std::byte{0}; bytes[33] = std::byte{3}; bytes[34] = std::byte{0}; bytes[35] = std::byte{10};
  bytes[36] = std::byte{0}; bytes[37] = std::byte{0}; bytes[38] = std::byte{0}; bytes[39] = std::byte{12};
  bytes[40] = std::byte{0}; bytes[41] = std::byte{12};
  bytes[44] = std::byte{0}; bytes[45] = std::byte{0}; bytes[46] = std::byte{0}; bytes[47] = std::byte{28};
  bytes[52] = std::byte{0}; bytes[53] = std::byte{0}; bytes[54] = std::byte{0}; bytes[55] = std::byte{1};
  for (int shift = 24, index = 56; shift >= 0; shift -= 8, ++index) bytes[index] = static_cast<std::byte>((first >> shift) & 0xffU);
  for (int shift = 24, index = 60; shift >= 0; shift -= 8, ++index) bytes[index] = static_cast<std::byte>((last >> shift) & 0xffU);
  bytes[64] = std::byte{0}; bytes[65] = std::byte{0}; bytes[66] = std::byte{0}; bytes[67] = std::byte{1};
  bytes[0] = std::byte{0}; bytes[1] = std::byte{1}; bytes[2] = std::byte{0}; bytes[3] = std::byte{0};
  return bytes;
}

void append_zgf_entry(std::vector<std::byte> &bytes, std::string_view name,
                      std::span<const std::byte> payload) {
  const auto start = bytes.size();
  append_u32_le(bytes, 1);
  append_u32_le(bytes, 0);
  append_u32_le(bytes, 0);
  append_u32_le(bytes, 1);
  append_u32_le(bytes, static_cast<std::uint32_t>(payload.size()));
  const auto padded_size = (payload.size() + 3U) & ~std::size_t{3};
  append_u32_le(bytes, static_cast<std::uint32_t>(8 + padded_size));
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  bytes.resize(start + 24 + padded_size);
  const auto name_offset = bytes.size() - start;
  append_text(bytes, name);
  bytes.push_back(std::byte{});
  bytes.resize((bytes.size() + 3U) & ~std::size_t{3});
  set_u32_le(bytes, start + 4,
             root_flag | static_cast<std::uint32_t>(bytes.size() - start));
  set_u32_le(bytes, start + 8, static_cast<std::uint32_t>(name_offset));
}

std::vector<std::byte>
packed_zgf(const std::vector<std::pair<std::string, std::vector<std::byte>>>
               &members) {
  std::vector<std::byte> decoded;
  append_u32_le(decoded, zgf_magic);
  append_u32_le(decoded, 0);
  append_u32_le(decoded, 0);
  append_u32_le(decoded, static_cast<std::uint32_t>(members.size()));
  for (const auto &[name, payload] : members)
    append_zgf_entry(decoded, name, payload);
  set_u32_le(decoded, 4,
             root_flag | static_cast<std::uint32_t>(decoded.size()));
  set_u32_le(decoded, 8, static_cast<std::uint32_t>(decoded.size()));

  std::vector<std::byte> packed;
  append_u32_le(packed, static_cast<std::uint32_t>(decoded.size()));
  append_u32_le(packed, static_cast<std::uint32_t>(decoded.size() + 9));
  packed.push_back(std::byte{1});
  packed.insert(packed.end(), decoded.begin(), decoded.end());
  return packed;
}

void write_zip(const std::filesystem::path &path,
               const std::vector<std::pair<std::string, std::vector<std::byte>>>
                   &members) {
  struct CentralEntry {
    std::string name;
    std::uint32_t crc{};
    std::uint32_t size{};
    std::uint32_t local_offset{};
  };
  std::vector<std::byte> bytes;
  std::vector<CentralEntry> entries;
  for (const auto &[name, payload] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
                static_cast<uInt>(payload.size())));
    const auto local_offset = static_cast<std::uint32_t>(bytes.size());
    append_u32_le(bytes, 0x04034b50U);
    append_u16_le(bytes, 20);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, crc);
    append_u32_le(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32_le(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u16_le(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16_le(bytes, 0);
    append_text(bytes, name);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    entries.push_back(
        {name, crc, static_cast<std::uint32_t>(payload.size()), local_offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : entries) {
    append_u32_le(bytes, 0x02014b50U);
    append_u16_le(bytes, 20);
    append_u16_le(bytes, 20);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, entry.crc);
    append_u32_le(bytes, entry.size);
    append_u32_le(bytes, entry.size);
    append_u16_le(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, entry.local_offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32_le(bytes, 0x06054b50U);
  append_u16_le(bytes, 0);
  append_u16_le(bytes, 0);
  append_u16_le(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u16_le(bytes, static_cast<std::uint16_t>(entries.size()));
  append_u32_le(bytes, central_size);
  append_u32_le(bytes, central_offset);
  append_u16_le(bytes, 0);

  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write synthetic font ZIP fixture");
}

std::string load_error(const std::filesystem::path &path) {
  try {
    static_cast<void>(off::ui::load_retail_ui_fonts(path));
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

  const auto first = valid_sfnt();
  const auto second = valid_sfnt(0x4f54544fU);
  const auto bundle = packed_zgf({{"ui/main.TTF", first},
                                  {"ui/readme.ttf.bak", first},
                                  {"ui/subtitle.oTf", second}});
  const auto valid_archive = work / "valid.zip";
  write_zip(valid_archive, {{"unrelated.txt", {std::byte{'x'}}},
                            {"startup/resources.ZgF", bundle}});
  auto loaded = off::ui::load_retail_ui_fonts(valid_archive);
  check(loaded.fonts.size() == 2,
        "discover only exact case-insensitive font extensions in the ZGF");
  check(loaded.fonts[0].sfnt == first && loaded.fonts[1].sfnt == second,
        "return exact owned in-memory font bytes in bundle order");
  const off::ui::RetailUiFontSet coverage{{{cmap12_sfnt(0x20U, 0x7eU)},
                                            {cmap12_sfnt(0x0400U, 0x04ffU)}}};
  check(off::ui::select_font_for_utf8(coverage, "Apply") == 0U,
        "font admission selects a single ASCII-capable font");
  check(off::ui::select_font_for_utf8(coverage, "Применить") == 1U,
        "font admission selects a single Cyrillic-capable font");
  check(!off::ui::select_font_for_utf8(coverage, "Apply Применить"),
        "font admission rejects text no single font can cover");
  const auto fallback=off::ui::select_font_runs_for_utf8(coverage, "Apply Применить");
  check(fallback && *fallback==std::vector<off::ui::RetailUiFontRun>{{0U,0U,6U},{1U,6U,18U}},
        "font fallback splits mixed-script UTF-8 into contiguous covering font runs");
  check(!off::ui::select_font_runs_for_utf8(coverage, std::string_view{"\xc3\x28",2}),
        "font fallback rejects malformed UTF-8 before selecting any run");
  const std::array<off::ui::FontRunRasterMetrics, 2> layout_metrics{{{20, 22, 16}, {30, 28, 21}}};
  const auto placements = off::ui::layout_ltr_font_runs(
      12.0F, 40.0F, layout_metrics);
  check(placements && *placements == std::vector<off::ui::FontRunPlacement>{{12.0F, 45.0F, 20.0F, 22.0F}, {32.0F, 40.0F, 30.0F, 28.0F}},
        "fallback runs advance in byte order while sharing the tallest baseline");
  const std::array<off::ui::FontRunRasterMetrics, 1> invalid_metrics{{{0, 10, 8}}};
  check(!off::ui::layout_ltr_font_runs(0.0F, 0.0F, invalid_metrics),
        "font-run layout rejects non-renderable raster metrics");
  check(!off::ui::select_font_for_utf8(coverage, std::string_view{"\xc3\x28", 2}),
        "font admission rejects malformed UTF-8");

  std::filesystem::remove(valid_archive, error);
  check(loaded.fonts[0].sfnt == first,
        "font storage remains valid after the source archive is removed");
  check(std::distance(std::filesystem::directory_iterator(work),
                      std::filesystem::directory_iterator{}) == 0,
        "loading retail fonts never extracts archive members to disk");

  const auto missing_archive = work / "missing.zip";
  write_zip(missing_archive, {{"startup/not-a-bundle.bin", bundle}});
  check(load_error(missing_archive) == "startup archive has no ZGF member",
        "reject an archive without a ZGF member");

  const auto duplicate_archive = work / "duplicate.zip";
  write_zip(duplicate_archive,
            {{"startup/a.zgf", bundle}, {"startup/b.ZGF", bundle}});
  check(load_error(duplicate_archive) ==
            "startup archive has multiple ZGF members",
        "reject multiple ZGF members instead of guessing");

  auto bad_font = valid_sfnt();
  bad_font[20] = std::byte{0xff};
  bad_font[21] = std::byte{0xff};
  bad_font[22] = std::byte{0xff};
  bad_font[23] = std::byte{0xf0};
  const auto invalid_archive = work / "invalid.zip";
  write_zip(invalid_archive, {{"startup/resources.zgf",
                               packed_zgf({{"ui/broken.ttf", bad_font}})}});
  check(load_error(invalid_archive) ==
            "startup archive has an invalid embedded font",
        "reject an sfnt table whose range leaves the owned payload");

  auto truncated_directory = valid_sfnt();
  truncated_directory.resize(20);
  check(!off::ui::is_bounded_sfnt(truncated_directory),
        "reject an sfnt with a truncated table directory");
  check(off::ui::is_bounded_sfnt(first),
        "accept a bounded TrueType sfnt structure");

  std::filesystem::remove_all(work, error);
  return failures == 0 ? 0 : 1;
}
