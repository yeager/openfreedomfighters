#include "off/data/picture_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <bit>
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

void append_f32(std::vector<std::byte>& bytes, float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

void append_texture_resource(std::vector<std::byte>& bytes, std::byte payload) {
    append_u32(bytes, 0x00020100U);
    for (std::size_t index = 4; index < 32; ++index) {
        bytes.push_back(payload);
    }
}

std::vector<std::byte> fixture(std::size_t prefix = 0, bool trailing = false) {
    std::vector<std::byte> bytes(16 + prefix, std::byte{0xee});
    const auto resource_offset = bytes.size();
    append_u32(bytes, 2);
    for (std::size_t index = 0; index < 2; ++index) {
        append_f32(bytes, 10.0F + static_cast<float>(index));
        append_f32(bytes, 20.0F + static_cast<float>(index));
        append_f32(bytes, static_cast<float>(index));
        append_f32(bytes, 0.125F);
        append_f32(bytes, 0.875F);
        append_f32(bytes, 0.75F);
        append_f32(bytes, 0.25F);
        append_f32(bytes, 8.0F + static_cast<float>(index));
        append_f32(bytes, 6.0F + static_cast<float>(index));
        append_u32(bytes, 0xff102030U + static_cast<std::uint32_t>(index));
    }
    append_u32(bytes, 2);
    const auto first_reference_position = bytes.size();
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 1);
    append_u32(bytes, 0);
    append_u32(bytes, 1);
    append_u32(bytes, 1);
    const auto first_texture_resource = bytes.size();
    append_texture_resource(bytes, std::byte{0x51});
    const auto second_texture_resource = bytes.size();
    append_texture_resource(bytes, std::byte{0x62});
    set_u32(
        bytes, first_reference_position,
        static_cast<std::uint32_t>(first_texture_resource)
    );
    set_u32(
        bytes, first_reference_position + 4,
        static_cast<std::uint32_t>(second_texture_resource)
    );
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
              picture.descriptors()[0].local_center_x == 10.0F &&
              picture.descriptors()[1].local_center_y == 21.0F &&
              picture.descriptors()[0].local_z == 0.0F &&
              picture.descriptors()[1].u_min == 0.125F &&
              picture.descriptors()[0].u_max == 0.875F &&
              picture.descriptors()[0].v_max == 0.75F &&
              picture.descriptors()[0].v_min == 0.25F &&
              picture.descriptors()[1].horizontal_edge_span == 9.0F &&
              picture.descriptors()[1].vertical_edge_span == 7.0F &&
              picture.descriptors()[0].modulation_color == 0xff102030U,
          "decode descriptor fields in source order");
    check(picture.texture_references().size() == 2 &&
              picture.texture_references()[0] == 128 &&
              picture.texture_references()[1] == 160,
          "preserve the draw-group texture-reference array");
    check(picture.texture_resources().size() == 2 &&
              picture.texture_resources()[0].prm_offset == 128 &&
              picture.texture_resources()[0].manager_key == 0x5151 &&
              picture.texture_resources()[0].encoded[4] ==
                  std::byte{0x51} &&
              picture.texture_resources()[1].prm_offset == 160 &&
              picture.texture_resources()[1].encoded[31] ==
                  std::byte{0x62},
          "resolve and own neutral draw-group texture-resource records");
    check(picture.draw_groups().size() == 2 &&
              picture.draw_groups()[0].descriptor_span_count == 1 &&
              picture.draw_groups()[0].first_descriptor_index == 0 &&
              picture.draw_groups()[1].descriptor_span_count == 1 &&
              picture.draw_groups()[1].first_descriptor_index == 1,
          "parse ordered draw-group descriptor spans");
    check(picture.encoded_size() + 84 == bytes.size(),
          "report the exact consumed resource prefix");
    bytes.assign(bytes.size(), std::byte{0});
    check(picture.descriptors()[1].local_center_x == 11.0F &&
              picture.texture_references()[1] == 160 &&
              picture.texture_resources()[0].encoded[4] ==
                  std::byte{0x51} &&
              picture.draw_groups()[0].descriptor_span_count == 1 &&
              picture.draw_groups()[0].first_descriptor_index == 0,
          "own parsed bytes independently of the source allocation");

    auto relocated = fixture(12, true);
    const auto relocated_picture = off::data::PictureResource::parse(relocated, 28);
    check(relocated_picture.encoded_size() + 98 == relocated.size(),
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
    check(empty.descriptors().empty() && empty.draw_groups().empty() &&
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

    auto odd_texture_reference = complete;
    set_u32(odd_texture_reference, 104, 129);
    check(rejects(odd_texture_reference),
          "reject an unaligned draw-group texture-resource reference");

    auto header_texture_reference = complete;
    set_u32(header_texture_reference, 104, 0);
    check(rejects(header_texture_reference),
          "reject a draw-group texture-resource reference into the PRM header");

    auto boundary_texture_reference = complete;
    const auto primitive_index = complete.size() - 4;
    set_u32(
        boundary_texture_reference, 104,
        static_cast<std::uint32_t>(primitive_index - 30)
    );
    check(rejects(boundary_texture_reference),
          "reject a truncated draw-group texture-resource at the PRM boundary");

    auto wrong_texture_marker = complete;
    set_u32(wrong_texture_marker, 128, 0x00020101U);
    check(rejects(wrong_texture_marker),
          "reject a draw-group texture-resource with the wrong type marker");

    auto duplicate_texture_reference = complete;
    set_u32(duplicate_texture_reference, 108, 128);
    const auto duplicate = off::data::PictureResource::parse(
        duplicate_texture_reference, 16
    );
    check(duplicate.texture_resources().size() == 2 &&
              duplicate.texture_resources()[0].prm_offset == 128 &&
              duplicate.texture_resources()[1].prm_offset == 128 &&
              duplicate.texture_resources()[1].encoded[4] ==
                  std::byte{0x51},
          "allow multiple draw groups to share one valid texture-resource record");

    auto hostile_descriptor_count = complete;
    set_u32(hostile_descriptor_count, 16, 0xffffffffU);
    check(rejects(hostile_descriptor_count), "reject a hostile descriptor count");

    auto hostile_group_count = complete;
    set_u32(hostile_group_count, 100, 0xffffffffU);
    check(rejects(hostile_group_count), "reject a hostile draw-group count");

    auto invalid_span = complete;
    set_u32(invalid_span, 112, 3);
    check(rejects(invalid_span), "reject a draw-group span past descriptor end");

    auto overflow_style_span = complete;
    set_u32(overflow_style_span, 112, 0xffffffffU);
    set_u32(overflow_style_span, 116, 1);
    check(rejects(overflow_style_span),
          "reject a hostile span without overflowing first plus count");

    auto late_invalid_descriptor = complete;
    set_u32(late_invalid_descriptor, 124, 3);
    check(rejects(late_invalid_descriptor),
          "validate every draw-group span before constructing output");

    auto zero_span_at_end = complete;
    set_u32(zero_span_at_end, 112, 0);
    set_u32(zero_span_at_end, 116, 2);
    const auto zero_span = off::data::PictureResource::parse(zero_span_at_end, 16);
    check(zero_span.draw_groups()[0].descriptor_span_count == 0 &&
              zero_span.draw_groups()[0].first_descriptor_index == 2,
          "accept a structurally bounded zero-length draw group");

    for (const std::size_t field_offset :
         {0U, 4U, 8U, 12U, 16U, 20U, 24U, 28U, 32U}) {
        for (const auto non_finite_bits :
             {0x7fc00000U, 0x7f800000U}) {
            auto non_finite = complete;
            set_u32(non_finite, 16U + 4U + field_offset, non_finite_bits);
            check(rejects(non_finite),
                  "reject NaN and infinity in every descriptor float field");
        }
    }
    for (const std::size_t span_offset : {28U, 32U}) {
        auto negative_span = complete;
        set_u32(negative_span, 16U + 4U + span_offset, 0xbf800000U);
        check(rejects(negative_span),
              "reject either negative descriptor edge span");
    }
}
