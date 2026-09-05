#include "off/data/packed_resource.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view fixture_text =
    "OpenFreedomFighters packed resource fixture\n";
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

std::vector<std::byte> deflated_fixture() {
    constexpr std::array encoded{
        0xf3, 0x2f, 0x48, 0xcd, 0x73, 0x2b, 0x4a, 0x4d, 0x4d, 0xc9, 0xcf, 0x75,
        0xcb, 0x4c, 0xcf, 0x28, 0x49, 0x2d, 0x2a, 0x56, 0x28, 0x48, 0x4c, 0xce,
        0x4e, 0x4d, 0x51, 0x28, 0x4a, 0x2d, 0xce, 0x2f, 0x2d, 0x4a, 0x4e, 0x55,
        0x48, 0xcb, 0xac, 0x28, 0x29, 0x2d, 0x4a, 0xe5, 0x02, 0x00,
    };
    std::vector<std::byte> bytes(9 + encoded.size() + 4);
    set_u32(bytes, 0, static_cast<std::uint32_t>(fixture_text.size()));
    set_u32(bytes, 4, static_cast<std::uint32_t>(bytes.size()));
    bytes[8] = std::byte{0};
    std::ranges::transform(encoded, bytes.begin() + 9, [](int value) {
        return static_cast<std::byte>(value);
    });
    constexpr std::array checksum{0x7e, 0xbc, 0x10, 0xd2};
    std::ranges::transform(
        checksum,
        bytes.begin() + 9 + encoded.size(),
        [](int value) { return static_cast<std::byte>(value); }
    );
    return bytes;
}

std::vector<std::byte> stored_fixture() {
    std::vector<std::byte> bytes(9 + fixture_text.size());
    set_u32(bytes, 0, static_cast<std::uint32_t>(fixture_text.size()));
    set_u32(bytes, 4, static_cast<std::uint32_t>(bytes.size()));
    bytes[8] = std::byte{1};
    std::ranges::transform(fixture_text, bytes.begin() + 9, [](char value) {
        return static_cast<std::byte>(value);
    });
    return bytes;
}

void check_payload(const off::data::PackedResource& resource, const char* message) {
    const auto payload = resource.payload();
    check(
        payload.size() == fixture_text.size() &&
            std::equal(payload.begin(), payload.end(), fixture_text.begin(), [](auto left, auto right) {
                return left == static_cast<std::byte>(right);
            }),
        message
    );
}

template <typename Mutation>
void check_rejected(Mutation mutation, const char* message) {
    auto bytes = deflated_fixture();
    mutation(bytes);
    bool rejected = false;
    try {
        static_cast<void>(off::data::PackedResource::parse(bytes));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto deflated = off::data::PackedResource::parse(deflated_fixture());
    check(deflated.encoding() == off::data::PackedResourceEncoding::deflate,
          "parse deflate encoding");
    check_payload(deflated, "inflate raw DEFLATE payload");

    const auto stored = off::data::PackedResource::parse(stored_fixture());
    check(stored.encoding() == off::data::PackedResourceEncoding::stored,
          "parse stored encoding");
    check_payload(stored, "copy stored payload");

    check_rejected(
        [](auto& value) { set_u32(value, 4, static_cast<std::uint32_t>(value.size() - 1)); },
        "reject a mismatched packed size"
    );
    check_rejected(
        [](auto& value) { set_u32(value, 0, 43); },
        "reject a mismatched inflated size"
    );
    check_rejected(
        [](auto& value) { value[8] = std::byte{2}; },
        "reject an unsupported encoding"
    );
    check_rejected(
        [](auto& value) { value[12] ^= std::byte{0xff}; },
        "reject a corrupt raw DEFLATE payload"
    );
    check_rejected(
        [](auto& value) { value.back() ^= std::byte{0xff}; },
        "reject an invalid Adler-32 checksum"
    );
    check_rejected(
        [](auto& value) {
            value.push_back(std::byte{0});
            set_u32(value, 4, static_cast<std::uint32_t>(value.size()));
        },
        "reject trailing compressed data"
    );

    return failures == 0 ? 0 : 1;
}
