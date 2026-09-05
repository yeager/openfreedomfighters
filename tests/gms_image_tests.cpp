#include "off/data/gms_image.hpp"

#include <algorithm>
#include <array>
#include <bit>
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

void set_f32(std::vector<std::byte>& bytes, std::size_t offset, float value) {
    set_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

std::vector<std::byte> packed_fixture() {
    std::vector<std::byte> payload(512);
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
    write_u32(80, 32);
    write_u32(80 + 16, 0x00100000U);
    write_u32(80 + 4, 384);
    write_u32(80 + 8, 420);
    write_u32(80 + 20, 432);
    write_u32(80 + 28, 16);
    write_u32(128, 2);
    write_u32(128 + 4, 1);
    write_u32(128 + 4 + 3 * 4, 1);
    write_u32(128 + 4 + 24 * 4 + 3 * 4, 1);
    write_u32(336 + 4, 384);
    write_u32(336 + 8, 420);
    write_u32(336, 40);
    for (std::size_t component = 0; component < 9; ++component) {
        set_f32(payload, 384 + component * 4, component % 4 == 0 ? 1.0F : 0.0F);
    }
    set_f32(payload, 420, 10.0F);
    set_f32(payload, 424, 20.0F);
    set_f32(payload, 428, 30.0F);
    write_u32(432, 1);
    write_u32(436, 444);
    set_f32(payload, 440, 2.0F);
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

std::vector<std::byte> deep_hierarchy_fixture() {
    std::vector<std::byte> payload(768);
    auto write = [&payload](std::size_t offset, std::uint32_t value) {
        for (unsigned int shift = 0; shift < 32; shift += 8) {
            payload[offset++] = static_cast<std::byte>((value >> shift) & 0xffU);
        }
    };
    write(0, 32); write(4, 744); write(12, 4); write(20, 128);
    write(32, 4);
    write(36, (1U << 24U) | 20U); write(40, 0);
    write(44, (1U << 24U) | 140U); write(48, 0);
    write(52, 155U); write(56, 0);
    write(60, (2U << 25U) | 155U); write(64, 0);
    write(744, 1); write(748, 752);
    payload[752] = std::byte{'x'};

    write(80 + 4, 680); write(80 + 8, 716);
    write(80 + 16, 0x00100000U);
    write(560 + 4, 680); write(560 + 8, 716);
    write(560 + 16, 0x00100000U);
    write(620 + 4, 680); write(620 + 8, 716);

    write(128, 3);
    write(128 + 4, 1);
    write(128 + 4 + 3 * 4, 1);
    write(128 + 4 + 24 * 4, 1);
    write(128 + 4 + 2 * 24 * 4 + 3 * 4, 1);
    for (std::size_t component = 0; component < 9; ++component) {
        write(680 + component * 4,
              std::bit_cast<std::uint32_t>(component % 4 == 0 ? 1.0F : 0.0F));
    }

    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

std::vector<std::byte> window_picture_fixture(
    bool has_extension = true,
    std::uint32_t authored_state_exponent = 0,
    std::uint32_t base_render_property = 2,
    std::uint32_t authored_alpha = 3,
    std::uint32_t alignment_enum = 4,
    std::uint32_t extension_control = 5
) {
    auto bytes = packed_fixture();
    constexpr std::size_t envelope_size = 9;
    constexpr std::size_t record_offset = 336;
    constexpr std::size_t block_offset = 452;
    constexpr std::uint32_t picture_source_type = 0x00200046U;

    set_u32(bytes, envelope_size + record_offset + 16, picture_source_type);
    set_u32(bytes, envelope_size + record_offset + 32, block_offset);
    set_u32(bytes, envelope_size + 128 + 4 + 4, 1);
    set_u32(bytes, envelope_size + 128 + 4 + 12, 0);
    set_u32(bytes, envelope_size + 128 + 4 + 24 * 4 + 4, 1);
    set_u32(bytes, envelope_size + 128 + 4 + 24 * 4 + 12, 0);

    const auto stream_size = static_cast<std::uint32_t>(
        sizeof(std::uint32_t) + 4U * 5U + (has_extension ? 5U : 0U) + 1U +
        5U + 1U + 1U
    );
    set_u32(bytes, envelope_size + block_offset, stream_size);
    auto cursor = envelope_size + block_offset + sizeof(std::uint32_t);
    auto append_scalar = [&](std::uint32_t value) {
        bytes[cursor] = std::byte{0x83};
        set_u32(bytes, cursor + 1U, value);
        cursor += 5U;
    };
    append_scalar(authored_state_exponent);
    append_scalar(base_render_property);
    append_scalar(authored_alpha);
    append_scalar(alignment_enum);
    if (has_extension) {
        append_scalar(extension_control);
    }
    bytes[cursor++] = std::byte{0x46};
    append_scalar(0x1234U);
    bytes[cursor++] = std::byte{0x06};
    bytes[cursor] = std::byte{0xff};
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
    check(image.decoded_size() == 512, "retain the decoded GMS image");
    check(image.directory().size() == 3, "parse the object-source directory");
    check(image.identifier_count() == 2, "parse the identifier table");
    check(image.pool_groups().size() == 2 &&
              image.pool_groups()[0].slot_count == 2 &&
              image.pool_groups()[1].slot_count == 1,
          "parse the pool-count table");
    check(image.hierarchy().size() == 3 &&
              image.hierarchy()[0].directory_index == 0 &&
              !image.hierarchy()[0].parent_directory_index.has_value() &&
              image.hierarchy()[0].children_in_directory_order ==
                  std::vector<std::size_t>{1} &&
              image.hierarchy()[1].parent_directory_index == 0 &&
              image.hierarchy()[1].children_in_directory_order.empty() &&
              !image.hierarchy()[2].parent_directory_index.has_value(),
          "materialize a nested child and pop back to a root sibling");
    std::size_t root_count = 0;
    std::size_t child_reference_count = 0;
    for (const auto& node : image.hierarchy()) {
        root_count += !node.parent_directory_index.has_value() ? 1U : 0U;
        if (node.parent_directory_index.has_value()) {
            check(*node.parent_directory_index < node.directory_index,
                  "require every hierarchy parent to precede its child");
        }
        child_reference_count += node.children_in_directory_order.size();
        check(std::ranges::is_sorted(node.children_in_directory_order),
              "preserve directory order among hierarchy siblings");
    }
    check(root_count == 2 && child_reference_count == 1,
          "represent every node exactly once as a root or child");

    auto sibling_bytes = packed_fixture();
    set_u32(sibling_bytes, 9 + 52, 84U);
    set_u32(sibling_bytes, 9 + 128 + 4 + 3 * 4, 0);
    set_u32(sibling_bytes, 9 + 128 + 4 + 24 * 4 + 3 * 4, 2);
    const auto sibling_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(sibling_bytes));
    check(sibling_image.hierarchy()[0].children_in_directory_order ==
              std::vector<std::size_t>({1, 2}) &&
              sibling_image.hierarchy()[1].parent_directory_index == 0 &&
              sibling_image.hierarchy()[2].parent_directory_index == 0,
          "preserve two nested siblings in serialized directory order");

    const auto deep_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(deep_hierarchy_fixture()));
    check(deep_image.directory()[3].parent_steps == 2 &&
              deep_image.hierarchy()[0].children_in_directory_order ==
                  std::vector<std::size_t>{1} &&
              deep_image.hierarchy()[1].parent_directory_index == 0 &&
              deep_image.hierarchy()[1].children_in_directory_order ==
                  std::vector<std::size_t>{2} &&
              deep_image.hierarchy()[2].parent_directory_index == 1 &&
              !deep_image.hierarchy()[3].parent_directory_index.has_value(),
          "pop pool and construction-parent stacks twice in lockstep");
    check(image.directory()[0].record_offset == 80 &&
              image.directory()[0].parent_steps == 0 &&
              image.directory()[0].pool_group == 0 &&
              image.directory()[0].pool_class == 0 &&
              image.directory()[0].class_ordinal == 0 &&
              image.directory()[0].group_slot_index == 0 &&
              image.directory()[0].local_slot_index == 0 &&
              image.directory()[0].enters_child_pool,
          "decode a packed object-source reference");
    check(image.directory()[0].basis ==
              std::array<float, 9>{1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                   0.0F, 0.0F, 0.0F, 1.0F} &&
              image.directory()[0].position ==
                  std::array<float, 3>{10.0F, 20.0F, 30.0F},
          "decode an object-source transform");
    check(image.directory()[0].buf_name_offset == 32 &&
              image.directory()[0].buf_auxiliary_offset == 16,
          "retain object-source BUF offsets");
    check(image.directory()[0].class_data_value == 0 &&
              !image.directory()[0].primitive_reference.has_value(),
          "retain class data without inventing a primitive reference");
    check(image.directory()[0].attachments.size() == 1 &&
              image.directory()[0].attachments[0].source_offset == 444 &&
              image.directory()[0].attachments[0].parameter == 2.0F,
          "decode an object-source attachment table");
    std::vector<std::byte> buf(64);
    set_u32(buf, 20, 12);
    image.validate_buf(buf);
    check(off::data::GmsImage::source_class_name(0x00200002U) == "ZSTDOBJ" &&
              off::data::GmsImage::source_class_name(0x00100021U) == "ZROOM" &&
              off::data::GmsImage::source_class_name(0x80800004U) == "ZLIGHT" &&
              !off::data::GmsImage::source_class_name(0xffffffffU).has_value(),
          "map source type codes to exported geometry classes");
    auto primitive_source_bytes = packed_fixture();
    set_u32(primitive_source_bytes, 9 + 336 + 12, 0x12345678U);
    set_u32(primitive_source_bytes, 9 + 336 + 16, 0x00200002U);
    set_u32(primitive_source_bytes, 9 + 128 + 4 + 4, 1);
    set_u32(primitive_source_bytes, 9 + 128 + 4 + 12, 0);
    set_u32(primitive_source_bytes, 9 + 128 + 4 + 24 * 4 + 4, 1);
    set_u32(primitive_source_bytes, 9 + 128 + 4 + 24 * 4 + 12, 0);
    const auto primitive_source_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(primitive_source_bytes)
    );
    check(primitive_source_image.directory()[1].class_data_value == 0x12345678U &&
              primitive_source_image.directory()[1].primitive_reference == 0x12345678U &&
              primitive_source_image.directory()[2].primitive_reference == 0x12345678U,
          "classify direct primitive references for geometry source types");
    const auto picture_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(window_picture_fixture())
    );
    check(picture_image.startup_window_picture_source(1).authored_state_exponent ==
                  0U &&
              picture_image.startup_window_picture_source(1).base_render_property ==
                  2U &&
              picture_image.startup_window_picture_source(1).authored_alpha == 3U &&
              picture_image.startup_window_picture_source(1).alignment_enum == 4U &&
              picture_image.startup_window_picture_source(1).extension_control == 5U &&
              picture_image.startup_window_picture_source(1).picture_asset_reference ==
                  0x1234U &&
              picture_image.startup_window_picture_source(2).authored_state_exponent ==
                  0U &&
              picture_image.startup_window_picture_source(2)
                      .picture_asset_reference == 0x1234U,
          "preserve authored state exponents and picture references");
    const auto picture_without_extension = off::data::GmsImage::parse(
        off::data::PackedResource::parse(window_picture_fixture(false))
    );
    check(picture_without_extension.startup_window_picture_source(1)
                  .picture_asset_reference == 0x1234U &&
              !picture_without_extension.startup_window_picture_source(1)
                   .extension_control.has_value(),
          "preserve absence of the optional extension scalar");
    const auto clamped_picture_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(
            window_picture_fixture(true, 0U, 0xfedcba98U, 999U, 15U, 99U))
    );
    const auto clamped_source =
        clamped_picture_image.startup_window_picture_source(1);
    check(clamped_source.base_render_property == 0xfedcba98U &&
              clamped_source.authored_alpha == 255U &&
              clamped_source.alignment_enum == 15U &&
              clamped_source.extension_control == 16U,
          "preserve the neutral property and clamp recovered authored controls");
    const auto persistent_picture_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(window_picture_fixture(true, 7U))
    );
    check(persistent_picture_image.startup_window_picture_source(1)
                  .authored_state_exponent == 7U,
          "preserve the highest authored exponent representable by a byte mask");
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
        [](auto& bytes) { set_u32(bytes, 9, 512); },
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
        [](auto& bytes) { set_u32(bytes, 9 + 64, 512); },
        "reject an out-of-bounds identifier"
    );
    check_parse_rejected(
        [](auto& bytes) {
            set_u32(bytes, 9 + 64, 511);
            bytes[9 + 511] = std::byte{1};
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
        [](auto& bytes) { set_u32(bytes, 9 + 80 + 4, 500); },
        "reject an out-of-bounds object-source basis"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 384, 0x7f800000U); },
        "reject a non-finite object-source basis"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 80 + 20, 510); },
        "reject an out-of-bounds attachment table"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 436, 512); },
        "reject an attachment target outside the image"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 440, 0x7f800000U); },
        "reject a non-finite attachment parameter"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 80 + 32, 512); },
        "reject a deferred source outside the image"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 80 + 40, 512); },
        "reject a post-load source outside the image"
    );
    check_rejected(
        [] {
            auto bytes = window_picture_fixture();
            set_u32(bytes, 9 + 452, 3);
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject an undersized tagged window-picture source"
    );
    check_rejected(
        [] {
            auto bytes = window_picture_fixture();
            bytes[9 + 452 + 4] = std::byte{2};
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject an unexpected window-picture scalar tag"
    );
    check_rejected(
        [] {
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(window_picture_fixture(true, 8U))
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject a window-picture state exponent outside a byte mask"
    );
    check_rejected(
        [] {
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(
                    window_picture_fixture(true, 0U, 0U, 0U, 16U))
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject a window-picture alignment enum outside its recovered range"
    );
    check_rejected(
        [] {
            auto bytes = window_picture_fixture();
            set_u32(bytes, 9 + 336 + 32, 0);
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject a window-picture source without deferred serialization"
    );
    check_rejected(
        [] {
            auto bytes = window_picture_fixture();
            set_u32(bytes, 9 + 452, 38);
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject trailing data in a tagged window-picture source"
    );
    check_rejected(
        [] {
            auto bytes = window_picture_fixture();
            bytes[9 + 452 + 36] = std::byte{0};
            const auto image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(image.startup_window_picture_source(1));
        },
        "reject a missing window-picture terminal tag"
    );
    check_rejected(
        [&image] { static_cast<void>(image.startup_window_picture_source(0)); },
        "reject a non-picture source at the startup parser boundary"
    );
    check_rejected(
        [&image] { static_cast<void>(image.startup_window_picture_source(3)); },
        "reject an out-of-range startup picture directory index"
    );
    check_rejected(
        [&image] {
            std::vector<std::byte> short_buf(16);
            image.validate_buf(short_buf);
        },
        "reject an object name outside its BUF resource"
    );
    check_rejected(
        [&image] {
            std::vector<std::byte> unterminated_buf(64, std::byte{1});
            set_u32(unterminated_buf, 20, 12);
            image.validate_buf(unterminated_buf);
        },
        "reject a non-terminated BUF object name"
    );
    check_rejected(
        [&image] {
            std::vector<std::byte> truncated_buf(64);
            set_u32(truncated_buf, 20, 60);
            image.validate_buf(truncated_buf);
        },
        "reject a truncated auxiliary BUF block"
    );
    check_parse_rejected(
        [](auto& bytes) { set_u32(bytes, 9 + 12, 3); },
        "reject an unsupported GMS format value"
    );

    return failures == 0 ? 0 : 1;
}
