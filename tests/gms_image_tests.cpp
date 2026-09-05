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
    std::vector<std::byte> payload(384);
    auto write_u32 = [&payload](std::size_t offset, std::uint32_t value) {
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            payload[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
        }
    };
    write_u32(0, 32);
    write_u32(4, 60);
    write_u32(12, 4);
    write_u32(20, 128);
    write_u32(32, 3);
    write_u32(36, (1U << 24U) | 20U);
    write_u32(40, 7);
    write_u32(44, 84U);
    write_u32(48, 0);
    write_u32(52, (1U << 25U) | 84U);
    write_u32(56, 0);
    write_u32(60, 2);
    write_u32(64, 72);
    write_u32(68, 324);
    const char first_identifier[] = "first";
    const char second_identifier[] = "second";
    std::copy_n(reinterpret_cast<const std::byte*>(first_identifier),
                sizeof(first_identifier), payload.begin() + 72);
    std::copy_n(reinterpret_cast<const std::byte*>(second_identifier),
                sizeof(second_identifier), payload.begin() + 324);
    write_u32(80 + 16, 0x00100000U);
    write_u32(128, 2);
    write_u32(128 + 4, 1);
    write_u32(128 + 4 + 3 * 4, 1);
    write_u32(128 + 4 + 24 * 4 + 3 * 4, 1);
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
    check(image.decoded_size() == 384, "retain the decoded GMS image");
    check(image.directory().size() == 3, "parse the object-source directory");
    check(image.identifier_count() == 2, "parse the identifier table");
    check(image.pool_groups().size() == 2 &&
              image.pool_groups()[0].slot_count == 2 &&
              image.pool_groups()[1].slot_count == 1,
          "parse the pool-count table");
    check(image.directory()[0].record_offset == 80 &&
              image.directory()[0].parent_steps == 0 &&
              image.directory()[0].pool_group == 0 &&
              image.directory()[0].pool_class == 0 &&
              image.directory()[0].class_ordinal == 0 &&
              image.directory()[0].group_slot_index == 0 &&
              image.directory()[0].local_slot_index == 0 &&
              image.directory()[0].enters_child_pool,
          "decode a packed object-source reference");
    check(image.directory()[1].pool_group == 1 &&
              image.directory()[1].pool_class == 3 &&
              image.directory()[1].group_slot_index == 0 &&
              image.directory()[1].local_slot_index == 2,
          "enter a child pool group");
    check(image.directory()[2].parent_steps == 1 &&
              image.directory()[2].pool_group == 0 &&
              image.directory()[2].pool_class == 3 &&
              image.directory()[2].group_slot_index == 1 &&
              image.directory()[2].local_slot_index == 1,
          "return to a parent pool group");
    check(image.local_source_for_handle(0x40000000U) == 0,
          "map runtime slot zero to its local source");
    check(image.local_source_for_handle(0x40000070U) == 2,
          "map a parent-group runtime slot to its local source");
    check(image.local_source_for_handle(0x400000e0U) == 1,
          "map a child-group runtime slot to its local source");
    check(!image.local_source_for_handle(0x40000150U).has_value(),
          "preserve a handle without a local source");
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
        [](auto& bytes) { set_u32(bytes, 9, 384); },
        "reject an out-of-bounds object-source directory"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 36, 95); },
        "reject a truncated object-source record"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 60, 100); },
        "reject an out-of-bounds identifier table"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 64, 384); },
        "reject an out-of-bounds identifier"
    );
    check_parse_rejected(
        [](auto& bytes) {
            set_u32(bytes, 9 + 64, 383);
            bytes[9 + 383] = std::byte{1};
        },
        "reject a non-terminated identifier"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 128, 4); },
        "reject an out-of-bounds pool-count table"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 128 + 4, 2); },
        "reject pool counts that do not cover the directory"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 52, (2U << 25U) | 84U); },
        "reject object-source hierarchy underflow"
    );
    check_parse_rejected(
        [](auto& bytes) { bytes[9 + 336 + 45] = std::byte{3}; },
        "reject an invalid source variant"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 12, 3); },
        "reject an unsupported GMS format value"
    );

    return failures == 0 ? 0 : 1;
}
