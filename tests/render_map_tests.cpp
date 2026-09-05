#include "off/data/render_map.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t table_offset = 48;
constexpr std::size_t descriptor_start = 80;
constexpr std::size_t descriptor_size = 84;
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void set_u16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

void set_f32(std::vector<std::byte>& bytes, std::size_t offset, float value) {
    set_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

std::vector<std::byte> map_fixture() {
    std::vector<std::byte> bytes(table_offset + 2 * (16 + descriptor_size), std::byte{0});
    set_u32(bytes, 0, table_offset);
    for (std::size_t index = 0; index < 3; ++index) {
        set_f32(bytes, 4 + index * 4, static_cast<float>(index + 1));
    }
    set_f32(bytes, 16, 0.25F);
    set_u16(bytes, 20, 0);
    set_u16(bytes, 22, 1);
    set_u16(bytes, 24, 0);
    set_u16(bytes, 26, 0x000bU);
    set_u16(bytes, 28, 3);
    set_u16(bytes, 30, 0);
    set_u16(bytes, 32, 0x800dU);
    set_u16(bytes, 34, 0);
    set_u16(bytes, 36, 1);
    set_u16(bytes, 38, 0x8002U);
    set_u16(bytes, 40, 0);
    set_u16(bytes, 42, 0);
    bytes[44] = std::byte{0x42};

    set_u32(bytes, table_offset, descriptor_start + descriptor_size);
    set_u32(bytes, table_offset + 16, descriptor_start);
    constexpr std::array minimums{
        std::array<std::uint16_t, 3>{20'000, 20'000, 10'000},
        std::array<std::uint16_t, 3>{30'000, 10'000, 30'000},
    };
    constexpr std::array maximums{
        std::array<std::uint16_t, 3>{25'000, 25'000, 15'000},
        std::array<std::uint16_t, 3>{35'000, 15'000, 35'000},
    };
    for (std::size_t entry = 0; entry < 2; ++entry) {
        const auto index = table_offset + entry * 16;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            set_u16(bytes, index + 4 + axis * 2, minimums[entry][axis]);
            set_u16(bytes, index + 10 + axis * 2, maximums[entry][axis]);
        }
        const auto descriptor = descriptor_start + entry * descriptor_size;
        set_u32(bytes, descriptor, static_cast<std::uint32_t>(entry));
        set_f32(bytes, descriptor + 4, 1.0F);
        set_f32(bytes, descriptor + 20, 1.0F);
        set_f32(bytes, descriptor + 36, 1.0F);
        set_f32(bytes, descriptor + 40, 100.0F + static_cast<float>(entry));
        set_f32(bytes, descriptor + 64, 5.0F);
        set_f32(bytes, descriptor + 68, 6.0F);
        set_f32(bytes, descriptor + 72, 7.0F);
        set_u32(
            bytes,
            descriptor + 76,
            0x40000100U + static_cast<std::uint32_t>(entry) * 0x100U
        );
        if (entry == 1) {
            set_u32(bytes, descriptor + 80, 0x40000300U);
        }
    }
    return bytes;
}

template <typename Mutation>
void check_rejected(Mutation mutation, const char* message) {
    auto bytes = map_fixture();
    mutation(bytes);
    bool rejected = false;
    try {
        static_cast<void>(off::data::RenderMap::parse(bytes));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto bytes = map_fixture();
    const auto map = off::data::RenderMap::parse(bytes);
    check(map.index_offset() == table_offset, "parse index offset");
    check(map.center() == std::array{1.0F, 2.0F, 3.0F}, "parse octree center");
    check(map.quantization_factor() == 0.25F, "parse quantization factor");
    check(map.nodes().size() == 4, "traverse the complete packed octree");
    check(map.nodes()[1].octant == 3, "decode node octant");
    check(map.nodes()[1].element_count == 1, "decode node element count");
    check(map.nodes()[1].child_index == 3, "decode node child index");
    check(!map.nodes()[1].last_sibling, "decode non-terminal sibling flag");
    check(map.nodes()[2].last_sibling, "decode terminal sibling flag");
    check(map.alignment_padding().size() == 4, "preserve alignment padding");
    check(map.entries().size() == 2, "parse map entries");
    check(map.entries()[0].descriptor_offset == descriptor_start + descriptor_size,
          "follow permuted descriptor index");
    check(map.entries()[0].bounds.minimum ==
              std::array<std::uint16_t, 3>{20'000, 20'000, 10'000},
          "parse quantized minimum");
    check(map.entries()[0].bounds.maximum ==
              std::array<std::uint16_t, 3>{25'000, 25'000, 15'000},
          "parse quantized maximum");
    check(map.entries()[0].object.kind == 1, "parse object kind");
    check(map.entries()[0].object.position[0] == 101.0F, "parse object position");
    check(map.entries()[0].object.extents == std::array{5.0F, 6.0F, 7.0F},
          "parse object extents");
    check(map.entries()[0].object.primary_geometry_reference == 0x40000200U,
          "parse the primary geometry reference");
    check(map.entries()[0].object.secondary_geometry_reference == 0x40000300U,
          "parse the optional secondary geometry reference");
    check(map.entries()[1].object.secondary_geometry_reference == 0,
          "preserve a null secondary geometry reference");

    check(
        map.query_bounds({
            .minimum = {-1'000'000.0F, -1'000'000.0F, -1'000'000.0F},
            .maximum = {1'000'000.0F, 1'000'000.0F, 1'000'000.0F},
        }) == std::vector<std::size_t>{0, 1},
        "query all octree entries in traversal order"
    );
    check(
        map.query_bounds({
            .minimum = {84'001.0F, 84'002.0F, 44'003.0F},
            .maximum = {88'001.0F, 88'002.0F, 48'003.0F},
        }) == std::vector<std::size_t>{0},
        "query a nested negative-z octant"
    );
    check(
        map.query_bounds({
            .minimum = {124'001.0F, 44'002.0F, 124'003.0F},
            .maximum = {128'001.0F, 48'002.0F, 128'003.0F},
        }) == std::vector<std::size_t>{1},
        "query a positive-z sibling octant"
    );
    check(
        map.query_bounds({
            .minimum = {1.0F, 2.0F, 3.0F},
            .maximum = {1.0F, 2.0F, 3.0F},
        }).empty(),
        "return no entries for a point outside indexed bounds"
    );
    bool invalid_query_rejected = false;
    try {
        static_cast<void>(map.query_bounds({
            .minimum = {2.0F, 0.0F, 0.0F},
            .maximum = {1.0F, 0.0F, 0.0F},
        }));
    } catch (const std::invalid_argument&) {
        invalid_query_rejected = true;
    }
    check(invalid_query_rejected, "reject inverted world-space query bounds");
    invalid_query_rejected = false;
    try {
        static_cast<void>(map.query_bounds({
            .minimum = {std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F},
            .maximum = {1.0F, 1.0F, 1.0F},
        }));
    } catch (const std::invalid_argument&) {
        invalid_query_rejected = true;
    }
    check(invalid_query_rejected, "reject non-finite world-space query bounds");

    check_rejected(
        [](auto& value) { set_u32(value, 0, table_offset + 1); },
        "reject a misaligned index offset"
    );
    check_rejected(
        [](auto& value) { set_f32(value, 16, 0.0F); },
        "reject a non-positive quantization factor"
    );
    check_rejected(
        [](auto& value) { set_u16(value, 28, 1); },
        "reject a reused hierarchy node"
    );
    check_rejected(
        [](auto& value) { set_u16(value, 32, 0x800bU); },
        "reject duplicate child octants"
    );
    check_rejected(
        [](auto& value) { set_u16(value, 38, 0x800aU); },
        "reject multiply referenced elements"
    );
    check_rejected(
        [](auto& value) {
            set_u16(value, 20, 0x0010U);
            set_u16(value, 22, 0);
        },
        "reject excessive hierarchy padding"
    );
    check_rejected(
        [](auto& value) { set_u16(value, 32, 0x8005U); },
        "reject an element missing from the hierarchy"
    );
    check_rejected(
        [](auto& value) { set_u32(value, table_offset + 16, descriptor_start + descriptor_size); },
        "reject duplicate descriptor references"
    );
    check_rejected(
        [](auto& value) { set_u32(value, table_offset, descriptor_start + 4); },
        "reject a misaligned descriptor reference"
    );
    check_rejected(
        [](auto& value) { set_u16(value, table_offset + 4, 30'000); },
        "reject inverted quantized bounds"
    );
    check_rejected(
        [](auto& value) { set_u32(value, descriptor_start, 2); },
        "reject an unsupported object kind"
    );
    check_rejected(
        [](auto& value) { set_u32(value, descriptor_start + 40, 0x7f800000U); },
        "reject a non-finite descriptor value"
    );
    check_rejected(
        [](auto& value) { set_u32(value, descriptor_start + 76, 0); },
        "reject a null primary geometry reference"
    );
    check_rejected(
        [](auto& value) { set_u32(value, descriptor_start + 76, 0x80000100U); },
        "reject an unsupported geometry-reference tag"
    );
    check_rejected(
        [](auto& value) { set_u32(value, descriptor_start + 80, 0x40000304U); },
        "reject a misaligned secondary geometry reference"
    );

    return failures == 0 ? 0 : 1;
}
