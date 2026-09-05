#include "off/data/render_map.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
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
    for (std::size_t index = 0; index < 4; ++index) {
        set_f32(bytes, 4 + index * 4, static_cast<float>(index + 1));
    }
    set_u32(bytes, 20, 0x00010000U);
    set_u32(bytes, 24, 0x80020000U);
    set_u32(bytes, 28, 3);
    bytes[32] = std::byte{0x42};

    set_u32(bytes, table_offset, descriptor_start + descriptor_size);
    set_u32(bytes, table_offset + 16, descriptor_start);
    for (std::size_t entry = 0; entry < 2; ++entry) {
        const auto index = table_offset + entry * 16;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            set_u16(bytes, index + 4 + axis * 2, static_cast<std::uint16_t>(10 + axis));
            set_u16(bytes, index + 10 + axis * 2, static_cast<std::uint16_t>(20 + axis));
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
        set_f32(bytes, descriptor + 76, 2.0F);
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
    check(map.root_parameters() == std::array{1.0F, 2.0F, 3.0F, 4.0F},
          "parse root parameters");
    check(map.quantization_scale() == 0x00010000U, "parse quantization scale");
    check(map.hierarchy_flags() == 0x80020000U, "preserve hierarchy flags");
    check(map.hierarchy_parameter() == 3, "preserve hierarchy parameter");
    check(map.packed_hierarchy().size() == 16, "preserve packed hierarchy");
    check(map.entries().size() == 2, "parse map entries");
    check(map.entries()[0].descriptor_offset == descriptor_start + descriptor_size,
          "follow permuted descriptor index");
    check(map.entries()[0].bounds.minimum == std::array<std::uint16_t, 3>{10, 11, 12},
          "parse quantized minimum");
    check(map.entries()[0].bounds.maximum == std::array<std::uint16_t, 3>{20, 21, 22},
          "parse quantized maximum");
    check(map.entries()[0].object.kind == 1, "parse object kind");
    check(map.entries()[0].object.position[0] == 101.0F, "parse object position");
    check(map.entries()[0].object.extents == std::array{5.0F, 6.0F, 7.0F},
          "parse object extents");

    check_rejected(
        [](auto& value) { set_u32(value, 0, table_offset + 1); },
        "reject a misaligned index offset"
    );
    check_rejected(
        [](auto& value) { set_u32(value, 20, 2); },
        "reject an unsupported quantization scale"
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
        [](auto& value) { set_u16(value, table_offset + 4, 30); },
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

    return failures == 0 ? 0 : 1;
}
