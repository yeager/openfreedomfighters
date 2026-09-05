#include "off/data/texture_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t dxt1_tag = 0x44585431U;
constexpr std::uint32_t dxt3_tag = 0x44585433U;
constexpr std::uint32_t abgr_tag = 0x52474241U;
constexpr std::uint32_t palette_tag = 0x50414c4eU;
constexpr std::size_t index_bytes = 2048 * sizeof(std::uint32_t);
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

void append_text(std::vector<std::byte>& bytes, std::string_view value) {
    const auto source = std::as_bytes(std::span{value.data(), value.size()});
    bytes.insert(bytes.end(), source.begin(), source.end());
    bytes.push_back(std::byte{0});
}

std::uint32_t mip_size(
    std::uint32_t tag,
    std::uint32_t width,
    std::uint32_t height
) {
    if (tag == dxt1_tag) {
        return ((width + 3U) / 4U) * ((height + 3U) / 4U) * 8U;
    }
    if (tag == dxt3_tag) {
        return ((width + 3U) / 4U) * ((height + 3U) / 4U) * 16U;
    }
    return width * height * (tag == abgr_tag ? 4U : 1U);
}

struct Fixture {
    std::vector<std::byte> bytes{16, std::byte{0}};
    std::array<std::size_t, 4> image_offsets{};
    std::size_t sequence_offset{0};
    std::size_t data_end{0};
};

void append_image(
    Fixture& fixture,
    std::size_t slot,
    std::uint32_t id,
    std::uint32_t tag,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mip_count,
    std::string_view name
) {
    auto& bytes = fixture.bytes;
    const auto start = bytes.size();
    fixture.image_offsets[slot] = start;
    append_u32(bytes, 0);
    append_u32(bytes, tag);
    append_u32(bytes, tag);
    append_u32(bytes, id);
    append_u32(bytes, width | (height << 16U));
    append_u32(bytes, mip_count);
    append_u32(bytes, 7);
    append_u32(bytes, 8);
    append_u32(bytes, 9);
    append_text(bytes, name);
    auto mip_width = width;
    auto mip_height = height;
    for (std::uint32_t level = 0; level < mip_count; ++level) {
        const auto size = mip_size(tag, mip_width, mip_height);
        append_u32(bytes, size);
        bytes.insert(bytes.end(), size, static_cast<std::byte>(level + 1));
        mip_width = mip_width > 1 ? mip_width / 2 : 1;
        mip_height = mip_height > 1 ? mip_height / 2 : 1;
    }
    if (tag == palette_tag) {
        append_u32(bytes, 2);
        append_u32(bytes, 0x11223344U);
        append_u32(bytes, 0xaabbccddU);
    }
    set_u32(bytes, start, static_cast<std::uint32_t>(bytes.size() - start));
}

Fixture catalog_fixture() {
    Fixture fixture;
    append_image(fixture, 0, 200, dxt1_tag, 4, 4, 1, "dxt-one");
    append_image(fixture, 1, 201, dxt3_tag, 4, 4, 1, "dxt-three");
    append_image(fixture, 2, 202, abgr_tag, 2, 2, 2, "color");
    append_image(fixture, 3, 203, palette_tag, 2, 2, 1, "indexed");
    fixture.sequence_offset = fixture.bytes.size();
    append_u32(fixture.bytes, 2);
    append_u32(fixture.bytes, 200);
    append_u32(fixture.bytes, 201);
    fixture.data_end = fixture.bytes.size();

    fixture.bytes.insert(fixture.bytes.end(), index_bytes, std::byte{0});
    for (std::size_t slot = 0; slot < fixture.image_offsets.size(); ++slot) {
        set_u32(
            fixture.bytes,
            fixture.data_end + (200 + slot) * sizeof(std::uint32_t),
            static_cast<std::uint32_t>(fixture.image_offsets[slot])
        );
    }
    const auto sequence_index = fixture.bytes.size();
    fixture.bytes.insert(fixture.bytes.end(), index_bytes, std::byte{0});
    set_u32(
        fixture.bytes,
        sequence_index + 200 * sizeof(std::uint32_t),
        static_cast<std::uint32_t>(fixture.sequence_offset)
    );
    set_u32(fixture.bytes, 0, static_cast<std::uint32_t>(fixture.data_end));
    set_u32(fixture.bytes, 4, static_cast<std::uint32_t>(sequence_index));
    set_u32(fixture.bytes, 8, 3);
    set_u32(fixture.bytes, 12, 4);
    return fixture;
}

template <typename Mutation>
void check_rejected(Mutation mutate, const char* message) {
    auto fixture = catalog_fixture();
    mutate(fixture);
    bool rejected = false;
    try {
        static_cast<void>(off::data::TextureCatalog::parse(fixture.bytes));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto fixture = catalog_fixture();
    const auto catalog = off::data::TextureCatalog::parse(fixture.bytes);
    check(catalog.images().size() == 4, "parse all four texture encodings");
    check(catalog.images()[0].id == 200, "preserve texture ID");
    check(catalog.images()[0].name == "dxt-one", "parse texture name");
    check(catalog.images()[0].mips[0].encoded.size() == 8, "parse DXT1 block");
    check(catalog.images()[1].mips[0].encoded.size() == 16, "parse DXT3 block");
    check(catalog.images()[2].mips.size() == 2, "parse ABGR mip chain");
    check(catalog.images()[2].mips[1].width == 1, "derive child mip dimensions");
    check(catalog.images()[3].palette.size() == 2, "parse indexed palette");
    check(catalog.images()[3].palette[1] == 0xaabbccddU, "preserve palette entry");
    check(catalog.images()[0].metadata == std::array<std::uint32_t, 3>{7, 8, 9},
          "preserve unknown interoperable metadata");
    check(catalog.sequences().size() == 1, "parse texture sequence");
    check(catalog.sequences()[0].id == 200, "recover indexed sequence ID");
    check(catalog.sequences()[0].texture_ids == std::vector<std::uint32_t>{200, 201},
          "parse sequence image references");

    std::vector<std::byte> empty(16 + index_bytes * 2, std::byte{0});
    set_u32(empty, 0, 16);
    set_u32(empty, 4, 16);
    set_u32(empty, 8, 3);
    set_u32(empty, 12, 4);
    empty[16 + index_bytes] = std::byte{0x7f};
    const auto empty_catalog = off::data::TextureCatalog::parse(empty);
    check(empty_catalog.images().empty(), "parse empty catalog variant");

    check_rejected([](auto& value) { set_u32(value.bytes, 8, 2); },
                   "reject catalog version mismatch");
    check_rejected([](auto& value) { set_u32(value.bytes, 4, 16); },
                   "reject invalid sequence-index offset");
    check_rejected(
        [](auto& value) {
            const auto descriptor = static_cast<std::uint32_t>(
                value.image_offsets[1] - value.image_offsets[0]
            );
            set_u32(value.bytes, value.image_offsets[0], descriptor | 0x40000000U);
        },
        "reject texture block flags"
    );
    check_rejected(
        [](auto& value) { set_u32(value.bytes, value.image_offsets[0] + 8, abgr_tag); },
        "reject disagreeing format tags"
    );
    check_rejected(
        [](auto& value) { set_u32(value.bytes, value.image_offsets[0] + 16, 0); },
        "reject zero texture dimensions"
    );
    check_rejected(
        [](auto& value) {
            const auto mip_header = value.image_offsets[0] + 4 + 32 + 8;
            set_u32(value.bytes, mip_header, 7);
        },
        "reject mismatched mip size"
    );
    check_rejected(
        [](auto& value) {
            const auto palette_count = value.image_offsets[3] + 4 + 32 + 8 + 4 + 4;
            set_u32(value.bytes, palette_count, 257);
        },
        "reject oversized palette"
    );
    check_rejected(
        [](auto& value) {
            const auto palette_index = value.image_offsets[3] + 4 + 32 + 8 + 4;
            value.bytes[palette_index] = std::byte{2};
        },
        "reject an out-of-range palette index"
    );
    check_rejected(
        [](auto& value) {
            set_u32(value.bytes, value.data_end + 200 * sizeof(std::uint32_t), 0);
        },
        "reject missing image-index entry"
    );
    check_rejected(
        [](auto& value) { set_u32(value.bytes, value.sequence_offset + 8, 1000); },
        "reject sequence reference to a missing image"
    );
    check_rejected(
        [](auto& value) {
            const auto sequence_index = value.data_end + index_bytes;
            set_u32(value.bytes, sequence_index + 200 * sizeof(std::uint32_t), 0);
        },
        "reject unindexed sequence block"
    );
    return failures == 0 ? 0 : 1;
}
