#include "off/graphics/startup_graphics_asset.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

constexpr std::size_t index_bytes = 2048 * sizeof(std::uint32_t);
static_assert(!std::is_default_constructible_v<off::graphics::StartupGraphicsAsset>);

void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
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

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

off::data::PictureDrawPlan plan(std::size_t count, std::size_t image_bias) {
  std::vector<off::data::PictureResourceDescriptor> descriptors(count);
  std::vector<off::data::PictureDrawGroup> groups;
  std::vector<off::data::PictureTextureBinding> bindings;
  for (std::size_t index = 0; index < count; ++index) {
    descriptors[index].horizontal_edge_span = 2;
    descriptors[index].vertical_edge_span = 2;
    descriptors[index].modulation_color = 0xffffffffU;
    groups.push_back({1, index});
    bindings.push_back({0, static_cast<std::uint16_t>(2048 + index),
                        static_cast<std::uint16_t>(index), image_bias + index,
                        off::data::TextureManagerKeyBank::upper});
  }
  return off::data::PictureDrawPlan::build(descriptors, groups, bindings);
}

off::data::StartupGraphicsComposition composition() {
  std::array<off::data::StartupGraphicsRowComposition, 8> rows;
  const auto background = plan(1, 0);
  const auto chrome = plan(5, 1);
  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    auto &row = rows[row_index];
    row.owner_directory_index = 100 + row_index;
    row.slot_y = static_cast<float>(row_index < 6 ? row_index * 18 : 108);
    row.same_slot_multiplicity = row_index < 6 ? 1 : 2;
    row.same_slot_ordinal = row_index < 6 ? 0 : row_index - 6;
    row.authored_hidden = row_index == 7;
    row.construction_chain = {row.owner_directory_index};
    row.transform_chain = {{row.owner_directory_index, {}, {}}};
    for (std::size_t picture_index = 0; picture_index < 3; ++picture_index) {
      auto &picture = row.pictures[picture_index];
      picture.role =
          picture_index == 0
              ? off::data::StartupGraphicsCompositionRole::row_background
              : off::data::StartupGraphicsCompositionRole::row_chrome;
      picture.directory_index = 200 + row_index * 3 + picture_index;
      picture.construction_chain = {row.owner_directory_index,
                                    picture.directory_index};
      picture.transform_chain = {{row.owner_directory_index, {}, {}},
                                 {picture.directory_index, {}, {}}};
      picture.authored_state_mask = picture_index == 0 ? 0x80U : 0x01U;
      picture.draw_plan = picture_index == 0 ? background : chrome;
    }
  }
  return off::data::StartupGraphicsComposition::from_rows(std::move(rows));
}

std::vector<std::byte> texture_catalog(std::uint32_t width = 2,
                                       std::uint32_t height = 2) {
  std::vector<std::byte> bytes(16, std::byte{0});
  std::array<std::size_t, 6> offsets{};
  for (std::size_t image = 0; image < offsets.size(); ++image) {
    offsets[image] = bytes.size();
    append_u32(bytes, 0);
    append_u32(bytes, 0x52474241U);
    append_u32(bytes, 0x52474241U);
    append_u32(bytes, static_cast<std::uint32_t>(image));
    append_u32(bytes, width | (height << 16U));
    append_u32(bytes, 1);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_text(bytes, "asset");
    bytes.push_back(std::byte{0});
    const auto mip_bytes = width * height * 4U;
    append_u32(bytes, mip_bytes);
    bytes.insert(bytes.end(), mip_bytes, static_cast<std::byte>(image + 1));
    set_u32(bytes, offsets[image],
            static_cast<std::uint32_t>(bytes.size() - offsets[image]));
  }
  const auto data_end = bytes.size();
  bytes.insert(bytes.end(), index_bytes, std::byte{0});
  for (std::size_t image = 0; image < offsets.size(); ++image)
    set_u32(bytes, data_end + image * 4,
            static_cast<std::uint32_t>(offsets[image]));
  const auto sequence_index = bytes.size();
  bytes.insert(bytes.end(), index_bytes, std::byte{0});
  set_u32(bytes, 0, static_cast<std::uint32_t>(data_end));
  set_u32(bytes, 4, static_cast<std::uint32_t>(sequence_index));
  set_u32(bytes, 8, 3);
  set_u32(bytes, 12, 4);
  return bytes;
}

void write_zip(const std::filesystem::path &path,
               const std::vector<std::pair<std::string, std::vector<std::byte>>>
                   &members) {
  struct Central {
    std::string name;
    std::uint32_t crc{};
    std::uint32_t size{};
    std::uint32_t offset{};
  };
  std::vector<std::byte> bytes;
  std::vector<Central> central;
  for (const auto &[name, payload] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
                static_cast<uInt>(payload.size())));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
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
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    central.push_back(
        {name, crc, static_cast<std::uint32_t>(payload.size()), offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : central) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, entry.size);
    append_u32(bytes, entry.size);
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write startup ZIP fixture");
}

bool load_rejects(const std::filesystem::path &path, std::string_view text) {
  try {
    static_cast<void>(off::graphics::load_startup_graphics_asset(path));
    return false;
  } catch (const std::runtime_error &error) {
    return std::string_view{error.what()}.find(text) != std::string_view::npos;
  }
}

} // namespace

int main(int argc, char **argv) {
  auto catalog_bytes = texture_catalog();
  const auto catalog = off::data::TextureCatalog::parse(catalog_bytes);
  auto asset =
      off::graphics::build_startup_graphics_asset(composition(), catalog);
  check(asset.images().size() == 6, "own exactly six decoded startup images");
  for (std::size_t index = 0; index < asset.images().size(); ++index) {
    check(asset.images()[index].catalog_image_index == index &&
              asset.images()[index].texture_id == index &&
              asset.images()[index].mip_zero.width == 2 &&
              asset.images()[index].mip_zero.height == 2 &&
              asset.images()[index].mip_zero.pixels.size() == 16,
          "retain sorted catalog-local identities and valid decoded pixels");
  }
  catalog_bytes.assign(catalog_bytes.size(), std::byte{0});
  check(asset.images()[5].mip_zero.pixels[0] == 6,
        "own decoded image bytes independently of source storage");

  bool budget_rejected = false;
  try {
    static_cast<void>(off::graphics::build_startup_graphics_asset(composition(),
                                                                  catalog, 95));
  } catch (const std::runtime_error &) {
    budget_rejected = true;
  }
  check(budget_rejected, "reject an aggregate decoded-byte budget overflow");
  check(off::graphics::build_startup_graphics_asset(composition(), catalog, 96)
                .images()
                .size() == 6,
        "accept an exact aggregate decoded-byte budget");

  auto bad_rows = composition().rows();
  bad_rows[7].pictures[2].draw_plan = plan(5, 0);
  bool signature_rejected = false;
  try {
    static_cast<void>(
        off::data::StartupGraphicsComposition::from_rows(std::move(bad_rows)));
  } catch (const std::runtime_error &) {
    signature_rejected = true;
  }
  check(signature_rejected, "reject a noncanonical six-image signature");

  const auto work = std::filesystem::path{OFF_TEST_WORK_DIR};
  std::filesystem::remove_all(work);
  std::filesystem::create_directories(work);
  const std::vector<std::byte> invalid{std::byte{0}};
  const auto missing = work / "missing.zip";
  write_zip(
      missing,
      {{"scene.gms", invalid}, {"scene.buf", invalid}, {"scene.prm", invalid}});
  check(load_rejects(missing, "no .tex member"),
        "reject a startup archive missing TEX");
  const auto duplicate = work / "duplicate.zip";
  write_zip(duplicate, {{"scene.gms", invalid},
                        {"other.GMS", invalid},
                        {"scene.buf", invalid},
                        {"scene.prm", invalid},
                        {"scene.tex", invalid}});
  check(load_rejects(duplicate, "duplicate .gms members"),
        "reject duplicate case-insensitive GMS members");
  const auto malformed = work / "malformed.zip";
  write_zip(malformed, {{"scene.gms", invalid},
                        {"scene.buf", invalid},
                        {"scene.prm", invalid},
                        {"scene.tex", invalid}});
  check(load_rejects(malformed, "packed-resource"),
        "reject malformed startup GMS before composition");
  std::filesystem::remove_all(work);
  if (argc == 2) {
    const auto retail = off::graphics::load_startup_graphics_asset(argv[1]);
    check(retail.images().size() == 6,
          "load six owned images from the verified retail startup archive");
  }
  std::cout << "startup graphics asset tests passed\n";
}
