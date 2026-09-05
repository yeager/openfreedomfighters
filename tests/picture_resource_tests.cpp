#include "off/data/picture_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
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

std::vector<std::byte> fixture(std::size_t prefix = 0, bool trailing = false) {
    std::vector<std::byte> bytes(16 + prefix, std::byte{0xee});
    const auto resource_offset = bytes.size();
    append_u32(bytes, 2);
    for (std::size_t index = 0; index < 80; ++index) {
        bytes.push_back(static_cast<std::byte>(index));
    }
    append_u32(bytes, 2);
    append_u32(bytes, 0x11);
    append_u32(bytes, 0x22);
    append_u32(bytes, 0x33);
    append_u32(bytes, 1);
    append_u32(bytes, 0x44);
    append_u32(bytes, 0);
    if (trailing) {
        bytes.push_back(std::byte{0xaa});
        bytes.push_back(std::byte{0xbb});
    }
    const auto index_offset = bytes.size();
    append_u32(bytes, 0);
    set_u32(bytes, 0, 16);
    set_u32(bytes, 4, static_cast<std::uint32_t>(index_offset));
    set_u32(bytes, 8, static_cast<std::uint32_t>(index_offset));
    set_u32(bytes, 12, 1);
    check(resource_offset <= 0xffffffffU, "keep the fixture relocation key portable");
    return bytes;
}

bool rejects(std::span<const std::byte> bytes, std::uint32_t key = 16) {
    try {
        static_cast<void>(off::data::PictureResource::parse(bytes, key));
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

}  // namespace

int main() {
    auto bytes = fixture();
    const auto picture = off::data::PictureResource::parse(bytes, 16);
    check(picture.descriptors().size() == 2 &&
              picture.descriptors()[0].encoded[0] == std::byte{0} &&
              picture.descriptors()[1].encoded[0] == std::byte{40},
          "copy opaque descriptors in source order");
    check(picture.frame_texture_references().size() == 2 &&
              picture.frame_texture_references()[0] == 0x11 &&
              picture.frame_texture_references()[1] == 0x22,
          "preserve the frame texture-reference array");
    check(picture.frames().size() == 2 &&
              picture.frames()[0].opaque_value == 0x33 &&
              picture.frames()[0].descriptor_index == 1 &&
              picture.frames()[1].opaque_value == 0x44 &&
              picture.frames()[1].descriptor_index == 0,
          "parse frame records and descriptor indexes");
    check(picture.encoded_size() + 20 == bytes.size(),
          "report the exact consumed resource prefix");
    bytes.assign(bytes.size(), std::byte{0});
    check(picture.descriptors()[1].encoded[0] == std::byte{40} &&
              picture.frame_texture_references()[1] == 0x22 &&
              picture.frames()[0].opaque_value == 0x33 &&
              picture.frames()[0].descriptor_index == 1,
          "own parsed bytes independently of the source allocation");

    auto relocated = fixture(12, true);
    const auto relocated_picture = off::data::PictureResource::parse(relocated, 28);
    check(relocated_picture.encoded_size() + 34 == relocated.size(),
          "accept unrelated allocation bytes outside the consumed prefix");

    std::vector<std::byte> empty_counts(16);
    append_u32(empty_counts, 0);
    append_u32(empty_counts, 0);
    set_u32(empty_counts, 0, 16);
    set_u32(empty_counts, 4, 24);
    set_u32(empty_counts, 8, 24);
    set_u32(empty_counts, 12, 1);
    append_u32(empty_counts, 0);
    const auto empty = off::data::PictureResource::parse(empty_counts, 16);
    check(empty.descriptors().empty() && empty.frames().empty() &&
              empty.encoded_size() == 8,
          "accept a structurally complete zero-count resource");

    const auto complete = fixture();
    for (std::size_t size = 0; size < complete.size(); ++size) {
        check(rejects(std::span<const std::byte>(complete).first(size)),
              "reject truncation at every byte boundary");
    }
    check(rejects(complete, static_cast<std::uint32_t>(complete.size() - 4)),
          "reject a relocation key at the primitive-index boundary");
    check(rejects(complete, 17), "reject an unaligned relocation key");

    auto hostile_descriptor_count = complete;
    set_u32(hostile_descriptor_count, 16, 0xffffffffU);
    check(rejects(hostile_descriptor_count), "reject a hostile descriptor count");

    auto hostile_frame_count = complete;
    set_u32(hostile_frame_count, 100, 0xffffffffU);
    check(rejects(hostile_frame_count), "reject a hostile frame count");

    auto invalid_descriptor = complete;
    set_u32(invalid_descriptor, 116, 2);
    check(rejects(invalid_descriptor),
          "reject a frame descriptor index equal to the descriptor count");

    auto late_invalid_descriptor = complete;
    set_u32(late_invalid_descriptor, 124, 2);
    check(rejects(late_invalid_descriptor),
          "validate every frame descriptor index before constructing output");
}
