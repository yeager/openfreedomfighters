#include "off/data/gms_image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

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
        bytes[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

std::vector<std::byte> packed_fixture() {
    std::vector<std::byte> payload(128);
    auto write_u32 = [&payload](std::size_t offset, std::uint32_t value) {
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            payload[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
        }
    };
    write_u32(0, 32);
    write_u32(4, 52);
    write_u32(12, 4);
    write_u32(32, 2);
    write_u32(36, (2U << 25U) | (1U << 24U) | 20U);
    write_u32(40, 7);
    write_u32(44, 20U);
    write_u32(48, 0);
    write_u32(52, 2);
    write_u32(56, 64);
    write_u32(60, 72);
    const char first_identifier[] = "first";
    const char second_identifier[] = "second";
    std::copy_n(reinterpret_cast<const std::byte*>(first_identifier),
                sizeof(first_identifier), payload.begin() + 64);
    std::copy_n(reinterpret_cast<const std::byte*>(second_identifier),
                sizeof(second_identifier), payload.begin() + 72);
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

template <typename Operation>
void check_rejected(Operation operation, const char* message) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

template <typename Mutation>
void check_parse_rejected(Mutation mutation, const char* message) {
    auto bytes = packed_fixture();
    mutation(bytes);
    check_rejected(
        [&bytes] {
            static_cast<void>(off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            ));
        },
        message
    );
}

}  // namespace

int main() {
    const auto image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(packed_fixture())
    );
    check(image.decoded_size() == 128, "retain the decoded GMS image");
    check(image.directory().size() == 2, "parse the object-source directory");
    check(image.identifier_count() == 2, "parse the identifier table");
    check(image.directory()[0].record_offset == 80 &&
              image.directory()[0].hierarchy_depth == 2 &&
              image.directory()[0].flagged,
          "decode a packed object-source reference");
    const auto beginning = off::data::GmsImage::decode_object_handle(0x40000000U);
    check(beginning.byte_offset == 0 && beginning.slot_index == 0,
          "decode a tagged zero-offset object handle");
    const auto middle = off::data::GmsImage::decode_object_handle(0x400000e0U);
    check(middle.byte_offset == 224 && middle.slot_index == 2,
          "decode a GMS object handle");

    check_rejected(
        [] { static_cast<void>(off::data::GmsImage::decode_object_handle(0)); },
        "reject a null object handle"
    );
    check_rejected(
        [] {
            static_cast<void>(
                off::data::GmsImage::decode_object_handle(0x80000070U)
            );
        },
        "reject an unsupported object-handle tag"
    );
    check_rejected(
        [] {
            static_cast<void>(
                off::data::GmsImage::decode_object_handle(0x40000010U)
            );
        },
        "reject a misaligned object handle"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9, 128); },
        "reject an out-of-bounds object-source directory"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 36, 31); },
        "reject a truncated object-source record"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 52, 20); },
        "reject an out-of-bounds identifier table"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 56, 128); },
        "reject an out-of-bounds identifier"
    );
    check_parse_rejected(
        [](auto& bytes) {
            set_u32(bytes, 9 + 56, 127);
            bytes[9 + 127] = std::byte{1};
        },
        "reject a non-terminated identifier"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 12, 3); },
        "reject an unsupported GMS format value"
    );

    return failures == 0 ? 0 : 1;
}
