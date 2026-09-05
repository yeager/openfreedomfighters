#include "off/data/primitive_catalog.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t descriptor_size = 124;
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
  for (unsigned int shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
  }
}

void set_u16(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint16_t value) {
  bytes[offset] = static_cast<std::byte>(value & 0xffU);
  bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned int shift = 0; shift < 32; shift += 8) {
    bytes[offset + shift / 8] =
        static_cast<std::byte>((value >> shift) & 0xffU);
  }
}

void set_f32(std::vector<std::byte> &bytes, std::size_t offset, float value) {
  set_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

struct Fixture {
  std::vector<std::byte> bytes{16, std::byte{0}};
  std::size_t topology_offset{0};
  std::size_t descriptor_offset{0};
  std::size_t reference_offset{0};
  std::size_t index_offset{0};
};

Fixture catalog_fixture(bool include_flagged_reference = false) {
  Fixture fixture;
  fixture.bytes.insert(fixture.bytes.end(), 3 * 36, std::byte{0});
  set_f32(fixture.bytes, 16, 1.0F);
  set_f32(fixture.bytes, 20, 2.0F);
  set_f32(fixture.bytes, 24, 3.0F);
  set_f32(fixture.bytes, 32, 1.0F);
  set_u32(fixture.bytes, 40, 0x80445566U);
  set_f32(fixture.bytes, 44, 0.25F);
  set_f32(fixture.bytes, 48, 0.75F);
  fixture.topology_offset = fixture.bytes.size();
  for (const std::uint16_t word : {1, 3, 0, 1, 2}) {
    append_u16(fixture.bytes, word);
  }

  fixture.descriptor_offset = fixture.bytes.size();
  fixture.bytes.insert(fixture.bytes.end(), descriptor_size, std::byte{0});
  set_u16(fixture.bytes, fixture.descriptor_offset, 0x0800);
  set_u16(fixture.bytes, fixture.descriptor_offset + 2, 3);
  set_u16(fixture.bytes, fixture.descriptor_offset + 4, 0x0812);
  set_u16(fixture.bytes, fixture.descriptor_offset + 14, 3);
  set_u32(fixture.bytes, fixture.descriptor_offset + 16, 16);
  set_u32(fixture.bytes, fixture.descriptor_offset + 20, 16);
  set_u32(fixture.bytes, fixture.descriptor_offset + 60,
          static_cast<std::uint32_t>(fixture.topology_offset));
  set_u32(fixture.bytes, fixture.descriptor_offset + 64, 5);

  if (include_flagged_reference) {
    fixture.reference_offset = fixture.bytes.size();
    fixture.bytes.insert(fixture.bytes.end(), descriptor_size, std::byte{0});
  }
  fixture.index_offset = fixture.bytes.size();
  append_u32(fixture.bytes,
             static_cast<std::uint32_t>(fixture.descriptor_offset));
  if (include_flagged_reference) {
    append_u32(fixture.bytes, 0x80000000U | static_cast<std::uint32_t>(
                                                fixture.reference_offset));
  }
  set_u32(fixture.bytes, 0, static_cast<std::uint32_t>(fixture.index_offset));
  set_u32(fixture.bytes, 4, static_cast<std::uint32_t>(fixture.index_offset));
  set_u32(fixture.bytes, 8, static_cast<std::uint32_t>(fixture.index_offset));
  set_u32(fixture.bytes, 12, include_flagged_reference ? 2 : 1);
  return fixture;
}

template <typename Mutation>
void check_rejected(Mutation mutation, const char *message) {
  auto fixture = catalog_fixture();
  mutation(fixture);
  bool rejected = false;
  try {
    static_cast<void>(off::data::PrimitiveCatalog::parse(fixture.bytes));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, message);
}

} // namespace

int main() {
  const auto fixture = catalog_fixture();
  const auto catalog = off::data::PrimitiveCatalog::parse(fixture.bytes);
  check(catalog.base_data_end() == fixture.index_offset,
        "preserve base-data boundary");
  check(catalog.entries().size() == 1, "parse primitive index");
  const auto &entry = catalog.entries().front();
  check(entry.descriptor_offset == fixture.descriptor_offset,
        "preserve descriptor offset");
  check(entry.format_flags == 0x0800, "preserve primitive format flags");
  check(entry.primitive_kind == 3, "preserve primitive kind");
  check(entry.texture_selector == 0x0812 && entry.texture_id == 0x12 &&
            entry.texture_selector_flagged,
        "decode primitive texture selector");
  check(entry.vertex_count == 3, "parse primitive vertex count");
  check(entry.vertex_data_offset == 16, "parse primitive vertex-data offset");
  check(entry.vertices.size() == 3, "decode primitive vertex table");
  check(entry.vertices[0].position == std::array{1.0F, 2.0F, 3.0F},
        "decode vertex position");
  check(entry.vertices[0].normal == std::array{0.0F, 1.0F, 0.0F},
        "decode vertex normal");
  check(entry.vertices[0].color_rgba ==
            std::array<std::uint8_t, 4>{0x44, 0x55, 0x66, 0x80},
        "decode packed vertex color to RGBA8");
  check(entry.vertices[0].texture_coordinates == std::array{0.25F, 0.75F},
        "decode vertex texture coordinates");
  check(entry.topology_data_offset == fixture.topology_offset,
        "parse primitive topology offset");
  check(entry.batches.size() == 1, "parse topology batch count");
  check(entry.batches[0].indices == std::vector<std::uint16_t>{0, 1, 2},
        "parse topology vertex indexes");

  const auto reference_fixture = catalog_fixture(true);
  const auto reference_catalog =
      off::data::PrimitiveCatalog::parse(reference_fixture.bytes);
  check(reference_catalog.entries().size() == 2, "parse flagged index entry");
  check(reference_catalog.entries()[1].flagged_reference,
        "preserve flagged primitive reference");
  check(reference_catalog.entries()[1].descriptor_offset ==
            reference_fixture.reference_offset,
        "mask flagged descriptor offset");

  auto overlapping_fixture = catalog_fixture(true);
  set_u32(overlapping_fixture.bytes, overlapping_fixture.index_offset + 4,
          0x80000000U | static_cast<std::uint32_t>(
                            overlapping_fixture.descriptor_offset + 2));
  bool overlapping_rejected = false;
  try {
    static_cast<void>(
        off::data::PrimitiveCatalog::parse(overlapping_fixture.bytes));
  } catch (const std::runtime_error &) {
    overlapping_rejected = true;
  }
  check(overlapping_rejected, "reject overlapping primitive descriptors");

  check_rejected(
      [](auto &value) { set_u32(value.bytes, 8, value.index_offset - 2); },
      "reject disagreeing index offsets");
  check_rejected([](auto &value) { set_u32(value.bytes, 12, 2); },
                 "reject an index table outside the file");
  check_rejected(
      [](auto &value) {
        set_u32(value.bytes, value.index_offset, 0x40000000U);
      },
      "reject an invalid packed descriptor offset");
  check_rejected(
      [](auto &value) { set_u16(value.bytes, value.descriptor_offset + 2, 2); },
      "reject an unsupported primitive kind");
  check_rejected(
      [](auto &value) {
        set_u16(value.bytes, value.descriptor_offset + 4, 0x1000);
      },
      "reject unsupported texture-selector bits");
  check_rejected(
      [](auto &value) { set_u16(value.bytes, value.descriptor_offset + 6, 1); },
      "reject nonzero texture-selector padding");
  check_rejected(
      [](auto &value) {
        set_u16(value.bytes, value.descriptor_offset + 14, 0);
      },
      "reject an empty vertex table");
  check_rejected(
      [](auto &value) {
        set_u32(value.bytes, value.descriptor_offset + 20, 17);
      },
      "reject an unaligned vertex-data offset");
  check_rejected([](auto &value) { set_u32(value.bytes, 16, 0x7f800000U); },
                 "reject a non-finite vertex component");
  check_rejected(
      [](auto &value) {
        set_u32(value.bytes, value.descriptor_offset + 64, 6);
      },
      "reject trailing topology words");
  check_rejected(
      [](auto &value) { set_u16(value.bytes, value.topology_offset + 8, 3); },
      "reject a topology reference outside the vertex table");

  return failures == 0 ? 0 : 1;
}
