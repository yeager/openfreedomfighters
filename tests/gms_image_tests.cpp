#include "off/data/gms_image.hpp"
#include "off/data/bounded_component_block_cursor.hpp"
#include "off/data/deferred_attachment_dispatch_shape.hpp"
#include "off/data/deferred_compact_block_profile.hpp"
#include "off/data/compact_typed_value_decoder.hpp"
#include "off/data/component_reader_context.hpp"
#include "off/data/deferred_component_dispatcher.hpp"
#include "off/data/deferred_reader_session.hpp"
#include "off/data/first_cut_owner_reader.hpp"
#include "off/data/first_cut_list_component_reader.hpp"
#include "off/data/first_cut_command_component_reader.hpp"
#include "off/data/first_cut_component_payload_session.hpp"
#include "off/data/keys_descriptor_range.hpp"
#include "off/data/keys_backing_evaluator.hpp"
#include "off/data/matpos_pose_evaluator.hpp"
#include "off/data/keys_property_materializer.hpp"
#include "off/data/owner_buf_keys_profile.hpp"
#include "off/data/scene_lifetime_keys_registry.hpp"
#include "off/data/typed_value_cursor.hpp"
#include "off/graphics/intro_named_global_section_envelope.hpp"
#include "off/runtime/owner_component_provider_binding.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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

class TestOwnerComponentProvider final : public off::runtime::OwnerComponentProvider {
public:
    [[nodiscard]] const void* find_child_exact(std::array<char, 4> key) const noexcept override {
        ++queries;
        return live && key == std::array<char, 4>{'K', 'E', 'Y', 'S'} ? &keys_child : nullptr;
    }

    mutable std::size_t queries{};
    bool live{true};
    std::uint32_t keys_child{0x4b455953U};
};

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

std::vector<std::byte> intro_controller_fixture(
    std::string_view destination = "SyntheticDestination", unsigned int optionals = 2
) {
    auto bytes = packed_fixture();
    bytes.resize(9U + 1024U);
    set_u32(bytes, 0, 1024U);
    set_u32(bytes, 4, 1033U);
    set_u32(bytes, 9U + 336U + 16U, 0x0800001aU);
    set_u32(bytes, 9U + 336U + 20U, 512U);
    set_u32(bytes, 9U + 336U + 32U, 600U);
    set_u32(bytes, 9U + 512U, 1U);
    set_u32(bytes, 9U + 516U, 544U);
    constexpr char identity[] = "ZGEOM_MovieControl";
    std::copy_n(reinterpret_cast<const std::byte*>(identity), sizeof(identity),
                bytes.begin() + 9U + 544U);
    std::vector<std::byte> block(4U);
    const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
        block.push_back(static_cast<std::byte>(tag));
        append_u32(block, value);
    };
    scalar(0x09U, 4U);
    block.push_back(std::byte{0x06});
    scalar(0x88U, 0xf1234567U);
    scalar(0x88U, 0x87654321U);
    scalar(0x08U, 0xffffffffU);
    block.push_back(std::byte{0x84});
    for (char c : destination) {
        block.push_back(static_cast<std::byte>(c));
    }
    block.push_back(std::byte{0});
    scalar(0x83U, 0x80000000U);
    if (optionals > 0U) scalar(0x88U, 0U);
    if (optionals > 1U) scalar(0x08U, 0xc1234567U);
    block.push_back(std::byte{0x06});
    block.push_back(std::byte{0xff});
    set_u32(block, 0, static_cast<std::uint32_t>(block.size()));
    std::copy(block.begin(), block.end(), bytes.begin() + 9U + 600U);
    bytes[9U + 600U + block.size()] = std::byte{0xa5};
    return bytes;
}

std::vector<std::byte> intro_list_fixture(const std::vector<std::uint32_t>& words) {
    auto bytes = intro_controller_fixture();
    set_u32(bytes, 9U + 336U + 20U, 0U);
    std::vector<std::byte> block(4U);
    block.push_back(std::byte{0x89});
    append_u32(block, static_cast<std::uint32_t>(4U + 4U * words.size()));
    for (const auto word : words) append_u32(block, word);
    block.push_back(std::byte{0x06});
    block.push_back(std::byte{0xff});
    set_u32(block, 0U, static_cast<std::uint32_t>(block.size()));
    std::copy(block.begin(), block.end(), bytes.begin() + 609U);
    bytes[609U + block.size()] = std::byte{0xa5};
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

// Independent grammar fixtures, deliberately unrelated to retail values.
std::vector<std::byte> cut_fixture(bool first, std::string_view name = "") {
    auto bytes = intro_controller_fixture();
    bytes.resize(4105U);
    set_u32(bytes, 0, 4096U); set_u32(bytes, 4, 4105U);
    set_u32(bytes, 9U + 336U + 32U, 1200U);
    set_u32(bytes, 521U, first ? 6U : 1U);
    std::size_t text = 700U;
    for (std::size_t i = 0; i < (first ? 6U : 1U); ++i) {
        const std::string_view identity = !first ? "ZLIST_CutSequence" :
            (i == 0 ? "ZLIST_CutSequenceList" : "ZLIST_CutSequenceCommand");
        set_u32(bytes, 525U + i * 8U, static_cast<std::uint32_t>(text));
        set_f32(bytes, 529U + i * 8U, first && i == 0 ? 0.0F : 1.0F);
        for (const auto c : identity) bytes[9U + text++] = static_cast<std::byte>(c);
        bytes[9U + text++] = std::byte{0};
    }
    std::vector<std::byte> block(4U);
    const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
        block.push_back(static_cast<std::byte>(tag)); append_u32(block, value);
    };
    scalar(0x89U, first ? 8U : 28U);
    for (std::uint32_t i = 0; i < (first ? 1U : 6U); ++i) append_u32(block, 0xf0000000U + i);
    block.push_back(std::byte{6});
    if (first) {
        constexpr std::array<std::uint8_t, 7> tags{0x83,0x83,3,8,3,0x83,3};
        for (std::size_t i = 0; i < tags.size(); ++i) scalar(tags[i], static_cast<std::uint32_t>(i + 8U));
        scalar(2, 0x80000000U);
        block.push_back(std::byte{6});
        for (std::uint32_t i = 0; i < 5; ++i) {
            scalar(i % 2U == 0U ? 3U : 0x83U, 9U - i);
            scalar(0x8aU, 0x80000000U + i); scalar(0x88U, i);
            scalar(i % 2U == 0U ? 0x83U : 3U, 100U + i);
            block.push_back(std::byte{4});
            for (const auto c : name) block.push_back(static_cast<std::byte>(c));
            block.push_back(std::byte{0}); block.push_back(std::byte{6});
        }
    } else {
        scalar(2, 0x80000000U); scalar(0x82U, std::bit_cast<std::uint32_t>(-17.5F));
        scalar(3, 0xffffffffU); block.push_back(std::byte{6});
    }
    block.push_back(std::byte{0xff});
    set_u32(block, 0, static_cast<std::uint32_t>(block.size()));
    std::copy(block.begin(), block.end(), bytes.begin() + 1209U);
    return bytes;
}

std::vector<std::byte> external_cut_commands_fixture() {
    auto bytes = intro_controller_fixture();
    bytes.resize(4105U);
    set_u32(bytes, 0, 4096U); set_u32(bytes, 4, 4105U);
    set_u32(bytes, 9U + 336U + 32U, 1200U);
    set_u32(bytes, 521U, 2U);
    std::size_t text = 700U;
    for (std::size_t index = 0; index < 2U; ++index) {
        constexpr std::string_view identity = "ZLIST_ExternCutSequenceCommand";
        set_u32(bytes, 525U + index * 8U, static_cast<std::uint32_t>(text));
        set_f32(bytes, 529U + index * 8U, 1.0F);
        for (const auto c : identity) bytes[9U + text++] = static_cast<std::byte>(c);
        bytes[9U + text++] = std::byte{0};
    }
    std::vector<std::byte> block(4U);
    const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
        block.push_back(static_cast<std::byte>(tag)); append_u32(block, value);
    };
    scalar(0x09U, 4U); block.push_back(std::byte{0x06});
    for (std::size_t index = 0; index < 2U; ++index) {
        scalar(0x83U, static_cast<std::uint32_t>(17U + index));
        scalar(0x8aU, static_cast<std::uint32_t>(0x80000020U + index));
        scalar(0x88U, index == 0U ? 0U : 0x80000004U);
        scalar(0x83U, static_cast<std::uint32_t>(70U + index));
        block.push_back(std::byte{0x04}); block.push_back(std::byte{0});
        scalar(0x88U, static_cast<std::uint32_t>(0x80000040U + index));
        block.push_back(std::byte{0x06});
    }
    block.push_back(std::byte{0xff});
    set_u32(block, 0, static_cast<std::uint32_t>(block.size()));
    std::copy(block.begin(), block.end(), bytes.begin() + 1209U);
    return bytes;
}

// Project-authored grammar fixture for the narrow StartLoader discovery path.
// It contains one ZWINGROUP/LoadScreen owner, not retail source data.
std::vector<std::byte> startloader_load_screen_fixture() {
    constexpr std::size_t envelope_size = 9U;
    std::vector<std::byte> payload(432U);
    set_u32(payload, 0U, 32U); set_u32(payload, 4U, 96U);
    set_u32(payload, 12U, 4U); set_u32(payload, 20U, 128U);
    set_u32(payload, 32U, 1U); set_u32(payload, 36U, 12U);
    set_u32(payload, 96U, 0U);
    set_u32(payload, 48U + 4U, 324U); set_u32(payload, 48U + 8U, 360U);
    set_u32(payload, 48U + 16U, 0x0010002eU); set_u32(payload, 48U + 20U, 372U);
    set_u32(payload, 48U + 32U, 384U);
    set_u32(payload, 128U, 2U); set_u32(payload, 132U, 1U);
    for (const auto offset : {324U, 340U, 356U}) set_f32(payload, offset, 0.0F);
    set_f32(payload, 324U, 1.0F); set_f32(payload, 340U, 1.0F); set_f32(payload, 356U, 1.0F);
    set_u32(payload, 372U, 1U); set_u32(payload, 376U, 100U); set_f32(payload, 380U, 0.0F);
    constexpr std::string_view identifier = "ZWINGROUP_LoadScreen";
    for (std::size_t index = 0; index < identifier.size(); ++index)
        payload[100U + index] = static_cast<std::byte>(identifier[index]);
    payload[100U + identifier.size()] = std::byte{0};
    std::vector<std::byte> block(4U);
    const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
        block.push_back(static_cast<std::byte>(tag)); append_u32(block, value);
    };
    scalar(3U, 0U); scalar(2U, std::bit_cast<std::uint32_t>(1.0F));
    scalar(3U, 1U); scalar(3U, 1U); scalar(3U, 0U);
    block.push_back(std::byte{6}); block.push_back(std::byte{6});
    block.push_back(std::byte{0x84});
    constexpr std::string_view target = "FF-Startup";
    for (const auto character : target) block.push_back(static_cast<std::byte>(character));
    block.push_back(std::byte{0}); block.push_back(std::byte{6}); block.push_back(std::byte{0xff});
    set_u32(block, 0U, static_cast<std::uint32_t>(block.size()));
    std::copy(block.begin(), block.end(), payload.begin() + 384);
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + envelope_size));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

void startloader_load_screen_tests() {
    const auto parse = [](auto bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes)));
    };
    const auto source = parse(startloader_load_screen_fixture()).startloader_load_screen_source();
    check(source.directory_index == 0U && source.target == "FF-Startup",
          "StartLoader discovery retains the unique authored target");
    check_rejected(
        [&] {
            auto bytes = startloader_load_screen_fixture();
            bytes[9U + 384U + 5U] = std::byte{4};
            static_cast<void>(parse(std::move(bytes)).startloader_load_screen_source());
        },
        "StartLoader discovery rejects altered deferred grammar");
    check_rejected(
        [&] {
            auto bytes = startloader_load_screen_fixture();
            set_f32(bytes, 9U + 380U, 1.0F);
            static_cast<void>(parse(std::move(bytes)).startloader_load_screen_source());
        },
        "StartLoader discovery rejects a nonzero attachment parameter");
}

void intro_window_tests() {
    const auto fixture = [] {
        auto bytes = intro_controller_fixture();
        set_u32(bytes, 9 + 336 + 16, 0x00100030U);
        set_u32(bytes, 9 + 336 + 20, 0U);
        // Every group-family directory occurrence advances the pool ordinal,
        // including both references to this window record without child flags.
        bytes.resize(1289U); set_u32(bytes, 0, 1280U); set_u32(bytes, 4, 1289U);
        set_u32(bytes, 9 + 20, 704U); set_u32(bytes, 9 + 704, 4U);
        set_u32(bytes, 9 + 708, 2U); set_u32(bytes, 9 + 804, 1U);
        std::vector<std::byte> block(4);
        const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
            block.push_back(static_cast<std::byte>(tag)); append_u32(block, value);
        };
        scalar(3, 0xfedcba98U); scalar(2, 0x80000000U); scalar(3, 7); scalar(3, 0xffffffffU); scalar(3, 23);
        block.push_back(std::byte{6}); block.push_back(std::byte{6});
        scalar(0x88, 0x80000003U); scalar(0x88, 0); scalar(0x88, 0xffffffffU);
        scalar(0x83, 11); scalar(0x83, 0); scalar(3, 0x87654321U);
        block.push_back(std::byte{6}); block.push_back(std::byte{0xff});
        set_u32(block, 0, static_cast<std::uint32_t>(block.size()));
        std::copy(block.begin(), block.end(), bytes.begin() + 609);
        return bytes;
    };
    const auto parse = [](auto bytes) { return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes))); };
    const auto decoded = parse(fixture()).intro_window_source(1);
    check(decoded.base_integer_a == 0xfedcba98U && decoded.base_integer_b == 23 &&
          std::bit_cast<std::uint32_t>(decoded.base_scalar) == 0x80000000U &&
          decoded.base_flag_a == 7 && decoded.base_flag_b == 0xffffffffU &&
          decoded.selected_camera_reference == 0x80000003U &&
          decoded.opaque_references == std::array<std::uint32_t, 2>{0, 0xffffffffU} &&
          decoded.options == std::array<std::uint32_t, 3>{11, 0, 0x87654321U},
          "window decoder preserves all raw words, references, truth values and signed zero");
    const auto reject_mutation = [&](auto mutate) {
        auto bad = fixture(); mutate(bad);
        check_rejected([&] { static_cast<void>(parse(bad).intro_window_source(1)); }, "malformed window source rejects");
    };
    for (std::uint32_t length = 0; length < 63; ++length)
        reject_mutation([&](auto& b) { set_u32(b, 609, length); });
    reject_mutation([](auto& b) { set_u32(b, 609, 64); });
    reject_mutation([](auto& b) { set_u32(b, 609, 0x0100003fU); });
    constexpr std::array<std::size_t, 15> tags{4,9,14,19,24,29,30,31,36,41,46,51,56,61,62};
    for (auto tag : tags) reject_mutation([&](auto& b) { b[609 + tag] ^= std::byte{0x40}; });
    for (auto nonfinite : {0x7f800000U, 0xff800000U, 0x7fc00000U})
        reject_mutation([&](auto& b) { set_u32(b, 619, nonfinite); });
    reject_mutation([](auto& b) { set_u32(b, 9 + 336 + 12, 1); });
    reject_mutation([](auto& b) { set_u32(b, 9 + 336 + 16, 0x00100000U); });
    reject_mutation([](auto& b) { set_u32(b, 9 + 336 + 20, 512); });
    reject_mutation([](auto& b) { set_u32(b, 9 + 336 + 32, 0); });
    reject_mutation([](auto& b) { set_u32(b, 9 + 336 + 32, 1022); });
    auto finite = fixture(); set_f32(finite, 619, -17.25F);
    check(parse(finite).intro_window_source(1).base_scalar == -17.25F, "window raw scalar is finite, not positive-only");
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_window_source(3)); }, "window index checked");
    check_rejected([&] { static_cast<void>(parse(intro_controller_fixture()).intro_window_source(1)); }, "controller cannot impersonate window");
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_camera_source(1)); }, "window does not relax camera guard");
}

void cut_tests() {
    const auto parse = [](auto bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes)));
    };
    const std::string raw_name = std::string(130U, 'x') + static_cast<char>(0xff);
    const auto cut = parse(cut_fixture(true, raw_name)).intro_first_cut_source(1U);
    check(cut.sequence_reference == 0xf0000000U && cut.settings_words[6] == 14U,
          "first cut retains base reference and settings");
    for (std::size_t i = 0; i < cut.settings_words.size(); ++i)
        check(cut.settings_words[i] == i + 8U, "retain every raw cut setting");
    const auto empty_names = parse(cut_fixture(true)).intro_first_cut_source(1U);
    for (const auto& command : empty_names.commands)
        check(command.target_name.empty(), "accept empty command target names");
    check(std::bit_cast<std::uint32_t>(cut.final_value) == 0x80000000U,
          "first cut preserves signed zero");
    for (std::size_t i = 0; i < 5; ++i) {
        check(cut.commands[i].timeline_position == 9U - i &&
              cut.commands[i].event_reference == 0x80000000U + i &&
              cut.commands[i].target_reference == i && cut.commands[i].event_argument == 100U + i &&
              cut.commands[i].target_name == raw_name, "command order and raw fields preserved");
    }
    const auto sequence = parse(cut_fixture(false)).intro_cut_sequence_source(1U);
    check(sequence.references[5] == 0xf0000005U && sequence.authored_option == 0xffffffffU &&
          sequence.values[1] == -17.5F && std::bit_cast<std::uint32_t>(sequence.values[0]) == 0x80000000U,
          "cut sequence preserves raw references, boolean and float pair");
    for (std::size_t i = 0; i < sequence.references.size(); ++i)
        check(sequence.references[i] == 0xf0000000U + i, "retain every cut resource reference");
    for (bool first : {false, true}) {
        const auto reject = [&](auto mutate) {
            auto bytes = cut_fixture(first); mutate(bytes);
            check_rejected([&] { const auto image = parse(bytes);
                if (first) static_cast<void>(image.intro_first_cut_source(1));
                else static_cast<void>(image.intro_cut_sequence_source(1));
            }, "cut malformed grammar rejected");
        };
        for (const auto offset : {357U, 361U, 377U}) reject([&](auto& b) { set_u32(b, offset, 1U); });
        reject([](auto& b) { set_u32(b, 521U, 0U); });
        reject([](auto& b) { set_u32(b, 525U, 4096U); });
        reject([](auto& b) { set_u32(b, 529U, 0x7fc00000U); });
        reject([](auto& b) { set_f32(b, 529U, 2.0F); });
        reject([](auto& b) { b[709U] = std::byte{'X'}; });
        reject([&](auto& b) { b[709U + (first ? 21U : 17U)] = std::byte{'X'}; });
        reject([](auto& b) { set_u32(b, 1209U, 0x01000033U); });
        reject([](auto& b) { set_u32(b, 1214U, 12U); });
        const std::uint32_t size = first ? 171U : 51U;
        std::vector<std::size_t> tags{4U, first ? 13U : 33U, size - 2U, size - 1U};
        if (first) {
            reject([](auto& b) { b[1223U] = std::byte{0x03}; });
            for (std::size_t i = 0; i < 8U; ++i) tags.push_back(14U + i * 5U);
            tags.push_back(54U);
            for (std::size_t i = 0; i < 5U; ++i) {
                for (const auto relative : {0U, 5U, 10U, 15U, 20U, 22U})
                    tags.push_back(55U + i * 23U + relative);
            }
            reject([](auto& b) { set_f32(b, 537U, 0.0F); });
            reject([](auto& b) { set_u32(b, 533U, 700U); });
            reject([](auto& b) { set_u32(b, 521U, 5U); });
            reject([](auto& b) { b[1264U] = std::byte{0xc3}; });
            reject([](auto& b) { std::fill(b.begin() + 1285U, b.begin() + 1380U, std::byte{0x41}); });
        } else {
            tags.insert(tags.end(), {34U, 39U, 44U});
        }
        for (const auto position : tags)
            reject([&](auto& b) { b[1209U + position] = std::byte{0x43}; });
        for (std::uint32_t shortened = 0; shortened < size; ++shortened)
            reject([&](auto& b) { set_u32(b, 1209U, shortened); });
        reject([&](auto& b) { set_u32(b, 1209U, size + 1U); });
        const std::size_t floating = first ? 1259U : 1244U;
        for (const auto nonfinite : {0x7f800000U, 0xff800000U, 0x7fc00000U})
            reject([&](auto& b) { set_u32(b, floating, nonfinite); });
        if (!first) {
            for (const auto nonfinite : {0x7f800000U, 0xff800000U, 0x7fc00000U})
                reject([&](auto& b) { set_u32(b, 1249U, nonfinite); });
        }
        const auto image = parse(cut_fixture(first));
        check_rejected([&] { if (first) static_cast<void>(image.intro_first_cut_source(99));
                            else static_cast<void>(image.intro_cut_sequence_source(99)); }, "cut bounds checked");
    }
    const auto external = parse(external_cut_commands_fixture())
                              .intro_external_cut_commands_source(1U);
    check(external.commands[0].timeline_position == 17U &&
              external.commands[0].event_reference == 0x80000020U &&
              external.commands[0].target_reference == 0U &&
              external.commands[0].event_argument == 70U &&
              external.commands[0].target_name.empty() &&
              external.external_list_references[0] == 0x80000040U &&
              external.commands[1].timeline_position == 18U &&
              external.commands[1].target_reference == 0x80000004U &&
              external.external_list_references[1] == 0x80000041U,
          "external cut command pair preserves ordered common fields and source references");
    const auto reject_external = [&](auto mutate) {
        auto bytes = external_cut_commands_fixture(); mutate(bytes);
        check_rejected([&] { static_cast<void>(parse(bytes).intro_external_cut_commands_source(1U)); },
                       "external cut command malformed grammar rejects");
    };
    for (const auto offset : {1213U, 1218U, 1219U, 1224U, 1229U, 1234U,
                              1239U, 1241U, 1246U, 1247U, 1252U, 1257U,
                              1262U, 1267U, 1269U, 1274U, 1275U})
        reject_external([&](auto &bytes) { bytes[offset] ^= std::byte{0x40}; });
    reject_external([](auto &bytes) { set_u32(bytes, 1209U, 0U); });
    reject_external([](auto &bytes) { set_u32(bytes, 1209U, 0x01000031U); });
    reject_external([](auto &bytes) { set_f32(bytes, 529U, 0.0F); });
    check_rejected([&] { static_cast<void>(parse(external_cut_commands_fixture())
                                               .intro_external_cut_commands_source(3U)); },
                   "external cut command source index checked");
    auto identifiers = packed_fixture();
    identifiers[9U + 72U] = std::byte{0xff};
    set_u32(identifiers, 9U + 68U, 72U);
    const auto image = parse(identifiers);
    check(!image.authored_event_identifier(0U) &&
          image.authored_event_identifier(1U) == image.authored_event_identifier(2U) &&
          image.authored_event_identifier(1U)->front() == static_cast<char>(0xff),
          "identifier join preserves raw bytes and duplicates");
    for (const auto raw : {3U, 0x80000001U, 0xffffffffU})
        check_rejected([&] { static_cast<void>(image.authored_event_identifier(raw)); }, "identifier bounds unmasked");
    const auto owned_identifier = parse(packed_fixture()).authored_event_identifier(1U);
    check(owned_identifier == "first", "identifier bytes outlive their source image");
    identifiers[9U + 72U] = std::byte{0};
    check(parse(identifiers).authored_event_identifier(1U) == "", "preserve empty event identifier");
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

void intro_camera_tests() {
    const auto set_double_bits = [](auto& bytes, std::size_t offset, std::uint64_t bits) {
        set_u32(bytes, offset, static_cast<std::uint32_t>(bits));
        set_u32(bytes, offset + 4U, static_cast<std::uint32_t>(bits >> 32U));
    };
    const auto fixture = [&] {
        auto bytes = intro_controller_fixture();
        set_u32(bytes, 9U + 336U + 16U, 0x00400003U);
        set_u32(bytes, 9U + 336U + 20U, 0U);
        set_u32(bytes, 609U, 122U);
        constexpr std::array<std::size_t, 4> double_positions{4, 13, 37, 46};
        constexpr std::array<double, 4> doubles{-2.25, 8192.125, 0.1234567890123, -135.5};
        for (std::size_t i = 0; i < doubles.size(); ++i) {
            bytes[609U + double_positions[i]] = i == 3U ? std::byte{0x81} : std::byte{1};
            set_double_bits(bytes, 610U + double_positions[i], std::bit_cast<std::uint64_t>(doubles[i]));
        }
        constexpr std::array<std::size_t, 10> integer_positions{22,27,32,55,60,65,70,75,90,115};
        constexpr std::array<std::uint8_t, 10> integer_tags{3,0x43,0x43,3,0x83,0x83,3,0x83,3,3};
        for (std::size_t i = 0; i < integer_positions.size(); ++i) {
            bytes[609U + integer_positions[i]] = static_cast<std::byte>(integer_tags[i]);
            set_u32(bytes, 610U + integer_positions[i], 0xf1234560U + static_cast<std::uint32_t>(i));
        }
        constexpr std::array<std::size_t, 6> float_positions{80,85,95,100,105,110};
        constexpr std::array<float, 6> floats{-3.5F, 7.25F, -0.25F, 1.5F, 2.25F, -4.0F};
        for (std::size_t i = 0; i < floats.size(); ++i) {
            bytes[609U + float_positions[i]] = i < 3U ? std::byte{2} : std::byte{0x42};
            set_f32(bytes, 610U + float_positions[i], floats[i]);
        }
        bytes[729U] = std::byte{6}; bytes[730U] = std::byte{0xff};
        bytes[731U] = std::byte{0xa5};
        return bytes;
    };
    const auto parse = [](auto bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes)));
    };
    const auto decode = [&](auto bytes) { return parse(std::move(bytes)).intro_camera_source(1U); };
    const auto camera = decode(fixture());
    check(camera.near_distance == -2.25 && camera.far_distance == 8192.125 &&
          camera.auxiliary_scalar == 0.1234567890123 && camera.angle_degrees == -135.5,
          "camera preserves original binary64 fields without clamping or degree conversion");
    check(camera.background_rgb == std::array<std::uint32_t, 3>{0xf1234560U,0xf1234561U,0xf1234562U} &&
          camera.integer_a == 0xf1234563U && camera.renderer_list_selector == 0xf1234564U &&
          camera.priority == 0xf1234565U && camera.aspect_mode == 0xf1234566U &&
          camera.flag_option_a == 0xf1234567U && camera.flag_option_b == 0xf1234568U &&
          camera.final_boolean == 0xf1234569U, "camera preserves full opaque integer words");
    check(camera.auxiliary_floats == std::array<float, 2>{-3.5F,7.25F} &&
          camera.viewport == std::array<float, 4>{-0.25F,1.5F,2.25F,-4.0F},
          "camera preserves finite raw floats without viewport composition or constraints");
    auto zero = fixture();
    for (const auto position : {4U,13U,37U,46U})
        set_double_bits(zero, 610U + position, 0x8000000000000000ULL);
    for (const auto position : {80U,85U,95U,100U,105U,110U})
        set_u32(zero, 610U + position, 0x80000000U);
    const auto signed_zero = decode(zero);
    for (const auto value : {signed_zero.near_distance, signed_zero.far_distance,
                             signed_zero.auxiliary_scalar, signed_zero.angle_degrees})
        check(std::bit_cast<std::uint64_t>(value) == 0x8000000000000000ULL, "preserve double negative zero");
    for (const auto value : signed_zero.auxiliary_floats)
        check(std::bit_cast<std::uint32_t>(value) == 0x80000000U, "preserve auxiliary float negative zero");
    for (const auto value : signed_zero.viewport)
        check(std::bit_cast<std::uint32_t>(value) == 0x80000000U, "preserve viewport float negative zero");
    auto limits = fixture();
    const double float_limit = static_cast<double>(std::numeric_limits<float>::max());
    set_double_bits(limits, 614U, std::bit_cast<std::uint64_t>(float_limit));
    set_double_bits(limits, 623U, std::bit_cast<std::uint64_t>(-float_limit));
    check(decode(limits).near_distance == float_limit && decode(limits).far_distance == -float_limit,
          "finite representable double boundary is retained, not narrowed");
    const auto reject = [&](auto mutation) {
        auto bytes = fixture(); mutation(bytes);
        check_rejected([&] { static_cast<void>(decode(bytes)); }, "malformed restricted camera rejected");
    };
    for (std::uint32_t size = 0; size < 122U; ++size)
        reject([&](auto& b) { set_u32(b, 609U, size); });
    for (const auto header : {123U,0x0100007aU,0x00ffffffU})
        reject([&](auto& b) { set_u32(b, 609U, header); });
    for (const auto position : {4U,13U,22U,27U,32U,37U,46U,55U,60U,65U,70U,75U,
                                80U,85U,90U,95U,100U,105U,110U,115U,120U,121U})
        reject([&](auto& b) { b[609U + position] ^= std::byte{0x40}; });
    for (const auto position : {4U,13U,37U,46U}) {
        for (const auto bits : {0x7ff0000000000000ULL,0xfff0000000000000ULL,0x7ff8000000000000ULL})
            reject([&](auto& b) { set_double_bits(b, 610U + position, bits); });
        for (const double value : {float_limit * 2.0, -float_limit * 2.0})
            reject([&](auto& b) { set_double_bits(b, 610U + position, std::bit_cast<std::uint64_t>(value)); });
    }
    for (const auto position : {80U,85U,95U,100U,105U,110U})
        for (const auto bits : {0x7f800000U,0xff800000U,0x7fc00000U})
            reject([&](auto& b) { set_u32(b, 610U + position, bits); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 12U, 1U); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 16U, 0x0800001aU); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 20U, 512U); });
    for (const auto offset : {0U,1023U})
        reject([&](auto& b) { set_u32(b, 9U + 336U + 32U, offset); });
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_camera_source(3U)); },
                   "camera directory index checked");
}

void intro_legal_picture_tests() {
    const auto fixture = [] {
        auto bytes = window_picture_fixture();
        bytes.resize(1033U);
        set_u32(bytes, 0U, 1024U); set_u32(bytes, 4U, 1033U);
        set_u32(bytes, 9U + 336U + 20U, 512U);
        set_u32(bytes, 9U + 336U + 32U, 600U);
        set_u32(bytes, 521U, 1U); set_u32(bytes, 525U, 544U);
        set_f32(bytes, 529U, 1.0F);
        constexpr char identity[] = "ZGEOM_Center";
        std::copy_n(reinterpret_cast<const std::byte*>(identity), sizeof(identity), bytes.begin() + 553U);
        set_u32(bytes, 609U, 38U);
        constexpr std::array<std::uint8_t, 5> tags{3,3,3,0x83,3};
        constexpr std::array<std::uint32_t, 5> values{6,0xfabc1234U,121,14,11};
        for (std::size_t i = 0; i < tags.size(); ++i) {
            bytes[613U + 5U * i] = static_cast<std::byte>(tags[i]);
            set_u32(bytes, 614U + 5U * i, values[i]);
        }
        bytes[638U] = std::byte{6}; bytes[639U] = std::byte{3};
        set_u32(bytes, 640U, 0xe1234567U);
        bytes[644U] = std::byte{6}; bytes[645U] = std::byte{6};
        bytes[646U] = std::byte{0xff}; bytes[647U] = std::byte{0xa5};
        return bytes;
    };
    const auto parse = [](auto bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes)));
    };
    const auto decode = [&](auto bytes) { return parse(std::move(bytes)).intro_legal_picture_source(1U); };
    const auto value = decode(fixture());
    check(value.authored_state_exponent == 6U && value.base_render_property == 0xfabc1234U &&
          value.authored_alpha == 121U && value.alignment_enum == 14U && value.extension_control == 11U &&
          value.picture_asset_reference == 0xe1234567U, "legal picture preserves reviewed fields and full resource key");
    auto maximum = fixture(); set_u32(maximum, 624U, 0xffffffffU); set_u32(maximum, 634U, 0xffffffffU);
    check(decode(maximum).authored_alpha == 255U && decode(maximum).extension_control == 16U,
          "legal picture unsigned alpha and extension clamps");
    auto zero = fixture(); set_u32(zero, 634U, 0U);
    check(decode(zero).extension_control == 0U, "legal picture zero extension remains present");
    const auto reject = [&](auto mutation) {
        auto bytes = fixture(); mutation(bytes);
        check_rejected([&] { static_cast<void>(decode(bytes)); }, "malformed legal picture rejected");
    };
    for (std::uint32_t size = 0; size < 38U; ++size)
        reject([&](auto& b) { set_u32(b, 609U, size); });
    for (const auto header : {39U,0x01000026U,0x00ffffffU})
        reject([&](auto& b) { set_u32(b, 609U, header); });
    for (const auto offset : {613U,618U,623U,628U,633U,638U,639U,644U,645U,646U})
        reject([&](auto& b) { b[offset] ^= std::byte{0x80}; });
    for (const auto count : {0U,2U}) reject([&](auto& b) { set_u32(b, 521U, count); });
    for (const auto parameter : {0U,0x80000000U,0x40000000U,0x7f800000U,0x7fc00000U})
        reject([&](auto& b) { set_u32(b, 529U, parameter); });
    reject([](auto& b) { b[553U] = std::byte{'X'}; });
    reject([](auto& b) { b[565U] = std::byte{'X'}; });
    reject([](auto& b) { set_u32(b, 525U, 1023U); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 12U, 1U); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 16U, 0U); });
    reject([](auto& b) { set_u32(b, 614U, 8U); });
    reject([](auto& b) { set_u32(b, 629U, 16U); });
    for (const auto offset : {0U,1023U})
        reject([&](auto& b) { set_u32(b, 9U + 336U + 32U, offset); });
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_legal_picture_source(3U)); }, "legal picture index checked");
    check_rejected([&] { static_cast<void>(parse(window_picture_fixture()).intro_legal_picture_source(1U)); },
                   "startup picture is not legal picture source");
    check_rejected([&] { static_cast<void>(parse(fixture()).startup_window_picture_source(1U)); },
                   "legal picture does not relax startup delimiters");
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_fade_picture_source(1U)); },
                   "center attachment is not fade attachment");
    auto fade = fixture();
    constexpr char fade_identity[] = "ZWINPIC_FadeToBlack";
    std::copy_n(reinterpret_cast<const std::byte*>(fade_identity), sizeof(fade_identity), fade.begin() + 553U);
    set_f32(fade, 529U, 0.0F); fade[618U] = std::byte{0x83};
    check(parse(fade).intro_fade_picture_source(1U).extension_control == 11U, "cross-check fixture has valid fade grammar");
    check_rejected([&] { static_cast<void>(decode(fade)); }, "fade picture is not legal picture source");
}

void intro_fade_picture_tests() {
    const auto fixture = [] {
        auto bytes = window_picture_fixture();
        bytes.resize(1033U);
        set_u32(bytes, 0U, 1024U); set_u32(bytes, 4U, 1033U);
        set_u32(bytes, 9U + 336U + 20U, 512U);
        set_u32(bytes, 9U + 336U + 32U, 600U);
        set_u32(bytes, 521U, 1U); set_u32(bytes, 525U, 544U);
        constexpr char identity[] = "ZWINPIC_FadeToBlack";
        std::copy_n(reinterpret_cast<const std::byte*>(identity), sizeof(identity),
                    bytes.begin() + 553U);
        set_u32(bytes, 609U, 38U);
        constexpr std::array<std::uint8_t, 5> tags{3, 0x83, 3, 0x83, 3};
        constexpr std::array<std::uint32_t, 5> values{7, 0xabcdef12U, 73, 15, 12};
        for (std::size_t i = 0; i < tags.size(); ++i) {
            bytes[613U + 5U * i] = static_cast<std::byte>(tags[i]);
            set_u32(bytes, 614U + 5U * i, values[i]);
        }
        bytes[638U] = std::byte{6}; bytes[639U] = std::byte{3};
        set_u32(bytes, 640U, 0xf1234567U);
        bytes[644U] = std::byte{6}; bytes[645U] = std::byte{6};
        bytes[646U] = std::byte{0xff}; bytes[647U] = std::byte{0xa5};
        return bytes;
    };
    const auto parse = [](auto bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(std::move(bytes)));
    };
    const auto decode = [&](auto bytes) { return parse(std::move(bytes)).intro_fade_picture_source(1U); };
    const auto value = decode(fixture());
    check(value.authored_state_exponent == 7U && value.base_render_property == 0xabcdef12U &&
              value.authored_alpha == 73U && value.alignment_enum == 15U &&
              value.extension_control == 12U && value.picture_asset_reference == 0xf1234567U,
          "intro fade picture preserves fields and ignores external padding");
    auto alternate = fixture(); alternate[633U] = std::byte{0x83};
    set_u32(alternate, 624U, 0xffffffffU); set_u32(alternate, 634U, 0xffffffffU);
    const auto clamped = decode(alternate);
    check(clamped.authored_alpha == 255U && clamped.extension_control == 16U,
          "intro fade picture uses unsigned clamps and alternate extension tag");
    auto zero = fixture(); set_u32(zero, 634U, 0U); set_u32(zero, 529U, 0x80000000U);
    check(decode(zero).extension_control == 0U, "mandatory zero extension and negative-zero parameter accepted");
    const auto reject = [&](auto mutation) {
        auto bytes = fixture(); mutation(bytes);
        check_rejected([&] { static_cast<void>(decode(bytes)); }, "malformed intro fade picture rejected");
    };
    for (std::uint32_t size = 0U; size < 38U; ++size)
        reject([&](auto& b) { set_u32(b, 609U, size); });
    for (const auto header : {39U, 0x01000026U, 0x00ffffffU})
        reject([&](auto& b) { set_u32(b, 609U, header); });
    for (const auto offset : {613U, 618U, 623U, 628U, 633U, 638U, 639U, 644U, 645U, 646U})
        reject([&](auto& b) { b[offset] = std::byte{0x43}; });
    reject([](auto& b) { b[613U] = std::byte{0x83}; });
    reject([](auto& b) { b[618U] = std::byte{3}; });
    reject([](auto& b) { b[639U] = std::byte{0x83}; });
    reject([](auto& b) { set_u32(b, 614U, 8U); });
    reject([](auto& b) { set_u32(b, 629U, 16U); });
    for (const auto count : {0U, 2U}) reject([&](auto& b) { set_u32(b, 521U, count); });
    for (const auto parameter : {0x3f800000U, 0x7f800000U, 0x7fc00000U})
        reject([&](auto& b) { set_u32(b, 529U, parameter); });
    reject([](auto& b) { b[553U] = std::byte{'X'}; });
    reject([](auto& b) { b[572U] = std::byte{'X'}; });
    reject([](auto& b) { set_u32(b, 525U, 1023U); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 12U, 1U); });
    reject([](auto& b) { set_u32(b, 9U + 336U + 16U, 0U); });
    for (const auto offset : {0U, 1023U})
        reject([&](auto& b) { set_u32(b, 9U + 336U + 32U, offset); });
    check_rejected([&] { static_cast<void>(parse(fixture()).intro_fade_picture_source(3U)); },
                   "intro fade picture index checked");
    check_rejected([&] { static_cast<void>(parse(window_picture_fixture()).intro_fade_picture_source(1U)); },
                   "startup grammar is not admitted as an intro fade picture");
    check_rejected([&] { static_cast<void>(parse(fixture()).startup_window_picture_source(1U)); },
                   "intro tail does not relax startup picture grammar");
}

}  // namespace

int main() {
    {
        using off::runtime::OwnerComponentProviderBinding;
        using off::runtime::OwnerComponentProviderBindings;

        TestOwnerComponentProvider provider;
        OwnerComponentProviderBindings bindings;
        OwnerComponentProviderBinding first;
        OwnerComponentProviderBinding second;
        check(!first.bound() && !first.invalidated() && bindings.size() == 0U,
              "factory-produced provider binding begins empty");

        first.bind(provider, bindings);
        second.bind(provider, bindings);
        const auto* first_child = first.find_required_keys_child();
        const auto* second_child = second.find_required_keys_child();
        check(first.bound() && second.bound() && bindings.size() == 2U &&
                  first_child == &provider.keys_child && second_child == &provider.keys_child &&
                  provider.queries == 2U,
              "provider binding performs exact borrowed KEYS lookup only after attachment binding");

        provider.live = false;
        check(first.find_required_keys_child() == nullptr && provider.queries == 3U,
              "provider binding does not cache a detached KEYS child result");
        provider.live = true;

        bool rejected_rebind = false;
        try {
            first.bind(provider, bindings);
        } catch (const std::runtime_error&) {
            rejected_rebind = true;
        }
        check(rejected_rebind, "provider binding rejects a live component rebind");

        first.invalidate();
        check(!first.bound() && first.invalidated() && bindings.size() == 1U &&
                  first.find_required_keys_child() == nullptr,
              "component teardown clears its borrowed provider before later use");

        bindings.invalidate_all();
        check(!second.bound() && second.invalidated() && bindings.size() == 0U &&
                  second.find_required_keys_child() == nullptr,
              "provider teardown invalidates every enrolled component before provider destruction");

        bool rejected_after_invalidation = false;
        try {
            second.bind(provider, bindings);
        } catch (const std::runtime_error&) {
            rejected_after_invalidation = true;
        }
        check(rejected_after_invalidation,
              "invalidated provider binding cannot silently rebind to a new owner lifetime");
    }
    {
        using off::data::CompactTypedValueDecoder;
        using off::data::CompactTypedValueKind;
        const std::array stream{
            std::byte{0x81}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
            std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
            std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0x3f},
            std::byte{0x88}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
            std::byte{0x44}, std::byte{'x'}, std::byte{0},
            std::byte{0x87}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{0xff},
        };
        CompactTypedValueDecoder decoder(stream);
        const auto wide = decoder.next();
        const auto scalar = decoder.next();
        const auto integer = decoder.next();
        const auto string = decoder.next();
        const auto length = decoder.next();
        const auto terminal = decoder.next();
        check(wide.kind == CompactTypedValueKind::binary64 && wide.value_class == 1U &&
                  wide.raw_tag == 0x81U && wide.binary64_bits() == 0x0807060504030201U &&
                  wide.encoded.size() == 9U,
              "compact decoder retains raw binary64 tag and little-endian payload bytes");
        check(scalar.kind == CompactTypedValueKind::binary32 && scalar.continuation &&
                  scalar.value_class == 2U && scalar.binary32_bits() == 0x3f800000U,
              "compact decoder ignores bit seven and retains bit-six continuation");
        check(integer.kind == CompactTypedValueKind::signed32 && integer.value_class == 8U &&
                  integer.signed32() == -1,
              "compact decoder admits recovered signed32 classes without assigning field semantics");
        check(string.kind == CompactTypedValueKind::nul_terminated_string && string.payload.size() == 2U &&
                  string.encoded.size() == 3U && length.kind == CompactTypedValueKind::length_u32 &&
                  length.u32_bits() == 16U && terminal.kind == CompactTypedValueKind::terminator &&
                  terminal.raw_tag == 0xffU && terminal.encoded.size() == 1U && decoder.empty(),
              "compact decoder bounds NUL strings, length values, and exact terminators");
        check_rejected([&] { static_cast<void>(wide.signed32()); },
                       "compact decoder rejects an incompatible typed representation");
        check_rejected([] {
            const std::array<std::byte, 1> unknown{std::byte{0x06}};
            CompactTypedValueDecoder decoder(unknown);
            static_cast<void>(decoder.next());
        }, "compact decoder does not advance an unrecovered class-six encoding");
        check_rejected([] {
            const std::array<std::byte, 2> truncated{std::byte{0x83}, std::byte{0}};
            CompactTypedValueDecoder decoder(truncated);
            static_cast<void>(decoder.next());
        }, "compact decoder rejects a truncated fixed-width payload");
        check_rejected([] {
            const std::array<std::byte, 2> unterminated{std::byte{0x84}, std::byte{'x'}};
            CompactTypedValueDecoder decoder(unterminated);
            static_cast<void>(decoder.next());
        }, "compact decoder rejects an unterminated string within its supplied boundary");
    }
    {
        using off::data::BoundedComponentBlockCursor;
        using off::data::DeferredComponentAttachmentSnapshot;
        const std::array stream{
            std::byte{0x03}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{0x06}, std::byte{0x04}, std::byte{'a'}, std::byte{0},
            std::byte{0x06}, std::byte{0xff}, std::byte{0xa5},
        };
        auto shared = std::span<const std::byte>(stream);
        BoundedComponentBlockCursor cursor(shared, stream.size() - 1U);
        DeferredComponentAttachmentSnapshot first;
        DeferredComponentAttachmentSnapshot second;
        const auto found_first = cursor.next_attachment(first);
        first.consume(3U);
        const auto found_second = cursor.next_attachment(second);
        const auto finished = !cursor.next_attachment(first);
        check(found_first && found_second && finished && first.remaining().size() == 8U &&
                  first.payload().size()==5U && second.payload().size()==3U &&
                  second.remaining().size() == 5U &&
                  second.remaining().front() == std::byte{0x04} && cursor.remaining().size() == 1U &&
                  shared.size() == 2U && shared.front() == std::byte{0xff},
              "bounded component block cursor snapshots before each delimiter, isolates readers, and leaves its terminator external");
        check_rejected([&] {
            auto short_shared = std::span<const std::byte>(stream);
            BoundedComponentBlockCursor short_cursor(short_shared, 10U);
            DeferredComponentAttachmentSnapshot ignored;
            static_cast<void>(short_cursor.next_attachment(ignored));
            static_cast<void>(short_cursor.next_attachment(ignored));
            static_cast<void>(short_cursor.next_attachment(ignored));
        }, "bounded component block cursor never reads a terminator outside its supplied block");
        using off::data::DeferredAttachmentDispatchClassifier;
        using off::data::DeferredAttachmentDispatchShape;
        const std::array<std::byte, 6> terminal_only{
            std::byte{0x03}, std::byte{0x11}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0xff}};
        const auto terminal = DeferredAttachmentDispatchClassifier::observe(terminal_only);
        const auto delimited = DeferredAttachmentDispatchClassifier::observe(
            std::span<const std::byte>(stream).first(stream.size() - 1U));
        check(terminal.shape == DeferredAttachmentDispatchShape::terminal_before_first_attachment_delimiter &&
                  terminal.delimiter_count == 0U &&
                  delimited.shape == DeferredAttachmentDispatchShape::attachment_delimiter_precedes_terminal &&
                  delimited.delimiter_count == 2U,
              "read-only dispatch classification distinguishes terminal owner blocks from attachment delimiters");
        const std::array<std::byte, 3> post_terminal{std::byte{0xff}, std::byte{0x06}, std::byte{0xff}};
        const std::array<std::byte, 2> high_bit_delimiter{std::byte{0x86}, std::byte{0xff}};
        check(DeferredAttachmentDispatchClassifier::observe(post_terminal).delimiter_count == 0U &&
                  DeferredAttachmentDispatchClassifier::observe(high_bit_delimiter).delimiter_count == 1U,
              "dispatch classification leaves terminal suffixes untouched and recognizes high-bit delimiters");
        using off::data::DeferredCompactBlockProfiler;
        const std::array compact_profiled{
            std::byte{0x03},std::byte{0x04},std::byte{0x00},std::byte{0x00},std::byte{0x00},std::byte{0x06},
            std::byte{0x43},std::byte{0x02},std::byte{0x00},std::byte{0x00},std::byte{0x00},
            std::byte{0x04},std::byte{'x'},std::byte{0x00},std::byte{0xff}};
        const auto compact_profile=DeferredCompactBlockProfiler::profile(compact_profiled);
        check(compact_profile.encoded_values==3U && compact_profile.attachment_delimiters==1U &&
                  compact_profile.continuation_values==1U && compact_profile.value_kinds[3U]==2U &&
                  compact_profile.value_kinds[4U]==1U,
              "deferred compact block profiler retains framing without decoding payload values");
        check_rejected([] {
            static_cast<void>(DeferredCompactBlockProfiler::profile(
                std::array{std::byte{0xff},std::byte{0x06}}));
        }, "deferred compact block profiler rejects bytes after the final terminator");
        check_rejected([] {
            const std::array<std::byte, 1> truncated{std::byte{0x03}};
            static_cast<void>(DeferredAttachmentDispatchClassifier::observe(truncated));
        }, "read-only dispatch classification rejects truncated compact values");
        check_rejected([] {
            const std::array<std::byte, 2> unknown{std::byte{0x7f}, std::byte{0xff}};
            static_cast<void>(DeferredAttachmentDispatchClassifier::observe(unknown));
        }, "read-only dispatch classification rejects unrecovered classes before its terminal");
    }
    {
        using off::data::DeferredComponentDispatcher;
        using off::data::DeferredComponentReader;
        const std::array stream{
            std::byte{0x83}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{0x06},
            std::byte{0x04}, std::byte{'a'}, std::byte{0},
            std::byte{0x06},
            std::byte{0x03}, std::byte{0x22}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{0xff}, std::byte{0xa5},
        };
        std::array<std::size_t, 2> received{};
        std::array<std::byte, 2> received_starts{};
        std::array<DeferredComponentReader, 2> readers{
            [&](std::span<const std::byte>& child) {
                received[0] = child.size();
                received_starts[0] = child.front();
                child = child.subspan(3U);
            },
            [&](std::span<const std::byte>& child) {
                received[1] = child.size();
                received_starts[1] = child.front();
                child = {};
            },
        };
        const auto result = DeferredComponentDispatcher::dispatch(stream, readers);
        check(result.dispatched_components == 2U && received[0] == 5U && received[1] == 3U &&
                  received_starts[0] == std::byte{0x83} && received_starts[1] == std::byte{0x04} &&
                  result.continuation.size() == 2U && result.continuation.front() == std::byte{0xff},
              "component dispatcher isolates each bounded payload and retains the terminator");
        {
            const std::array terminal_only{std::byte{0xff}, std::byte{0xa5}};
            const std::array<DeferredComponentReader, 0> no_readers{};
            const auto terminal = DeferredComponentDispatcher::dispatch(terminal_only, no_readers);
            check(terminal.dispatched_components == 0U && terminal.continuation.size() == 2U &&
                      terminal.continuation.front() == std::byte{0xff},
                  "component dispatcher recognizes a terminal before creating an attachment snapshot");
        }
        {
            const std::array continued_stream{
                std::byte{0xc3}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                std::byte{0x86}, std::byte{0xff},
            };
            std::size_t received_size = 0U;
            const std::array<DeferredComponentReader, 1> continued_readers{
                [&](std::span<const std::byte>& child) { received_size = child.size(); },
            };
            const auto continued = DeferredComponentDispatcher::dispatch(continued_stream, continued_readers);
            check(received_size == 5U && continued.dispatched_components == 1U &&
                  continued.continuation.size() == 1U && continued.continuation.front() == std::byte{0xff},
                  "component dispatcher bounds readers before later delimiters and leaves the terminator external");
        }
        check_rejected([&] {
            const std::array bad{std::byte{0x06}, std::byte{0xff}};
            const std::array<DeferredComponentReader, 0> none{};
            static_cast<void>(DeferredComponentDispatcher::dispatch(bad, none));
        }, "component dispatcher rejects an unmatched delimiter");
        check_rejected([&] {
            const std::array bad{std::byte{0x06}, std::byte{0xff}};
            const std::array<DeferredComponentReader, 2> too_many{[](auto&) {}, [](auto&) {}};
            static_cast<void>(DeferredComponentDispatcher::dispatch(bad, too_many));
        }, "component dispatcher rejects a missing attachment delimiter");
        check_rejected([&] {
            const std::array bad{std::byte{0x86}, std::byte{0xff}};
            const std::array<DeferredComponentReader, 0> none{};
            static_cast<void>(DeferredComponentDispatcher::dispatch(bad, none));
        }, "component dispatcher compares delimiters by their low-six-bit class");
        check_rejected([&] {
            const std::array bad{std::byte{0x09}, std::byte{0xff}};
            const std::array<DeferredComponentReader, 0> none{};
            static_cast<void>(DeferredComponentDispatcher::dispatch(bad, none));
        }, "component dispatcher rejects unknown advancing tags");
        check_rejected([&] {
            const std::array bad{std::byte{0x03}, std::byte{0x00}};
            const std::array<DeferredComponentReader, 0> none{};
            static_cast<void>(DeferredComponentDispatcher::dispatch(bad, none));
        }, "component dispatcher rejects truncated generic values");
    }
    {
        using off::data::FirstCutComponentPayloadSession;
        std::vector<std::byte> stream;
        const auto scalar=[&](std::uint8_t tag,std::uint32_t value) {
            stream.push_back(static_cast<std::byte>(tag));
            append_u32(stream,value);
        };
        constexpr std::array<std::uint8_t,7> list_tags{0x83U,0x83U,0x03U,0x08U,0x03U,0x83U,0x03U};
        for(std::size_t index=0;index<list_tags.size();++index)
            scalar(list_tags[index],static_cast<std::uint32_t>(index+1U));
        scalar(0x02U,0x80000000U); stream.push_back(std::byte{0x06});
        for(std::uint32_t index=0;index<5U;++index) {
            scalar(index%2U==0U?0x03U:0x83U,10U+index);
            scalar(0x8aU,20U+index); scalar(0x88U,30U+index);
            scalar(index%2U==0U?0x83U:0x03U,40U+index);
            stream.push_back(std::byte{0x04}); stream.push_back(std::byte{0}); stream.push_back(std::byte{0x06});
        }
        stream.push_back(std::byte{0xff});
        const off::data::DeferredOwnerReaderResult owner_result{stream,stream.size()};
        const auto parsed=FirstCutComponentPayloadSession::read(owner_result);
        check(stream.size()==157U && parsed.list.controls.front()==1U &&
                  std::bit_cast<std::uint32_t>(parsed.list.final_value)==0x80000000U &&
                  parsed.commands.front().timeline_position==10U && parsed.commands.back().event_argument==44U &&
                  parsed.commands.front().target_name.empty(),
              "first-cut component payload session reads exactly one list and five ordered commands");
        check_rejected([&] {
            auto malformed=stream;
            malformed.push_back(std::byte{0});
            static_cast<void>(FirstCutComponentPayloadSession::read(malformed,malformed.size()));
        }, "first-cut component payload session rejects a suffix beyond its reviewed extent");
    }
    {
        using off::data::FirstCutCommandComponentReader;
        std::vector<std::byte> payload;
        const auto scalar=[&](std::uint8_t tag,std::uint32_t value) {
            payload.push_back(static_cast<std::byte>(tag));
            append_u32(payload,value);
        };
        scalar(0x83U,9U); scalar(0x8aU,10U); scalar(0x88U,11U); scalar(0x03U,12U);
        payload.push_back(std::byte{0x04}); payload.push_back(std::byte{'o'}); payload.push_back(std::byte{'k'});
        payload.push_back(std::byte{0});
        const auto record=FirstCutCommandComponentReader::read(payload);
        check(record.timeline_position==9U && record.event_reference==10U && record.target_reference==11U &&
                  record.event_argument==12U && record.target_name=="ok",
              "first-cut command component reader retains one bounded command record");
        check_rejected([&] {
            auto malformed=payload;
            malformed[0]=std::byte{0x04};
            static_cast<void>(FirstCutCommandComponentReader::read(malformed));
        }, "first-cut command component reader rejects an unsupported integer tag");
        check_rejected([&] {
            auto malformed=payload;
            malformed.pop_back();
            static_cast<void>(FirstCutCommandComponentReader::read(malformed));
        }, "first-cut command component reader rejects an unterminated name");
    }
    {
        using off::data::FirstCutListComponentReader;
        std::vector<std::byte> payload;
        const auto scalar=[&](std::uint8_t tag,std::uint32_t value) {
            payload.push_back(static_cast<std::byte>(tag));
            append_u32(payload,value);
        };
        constexpr std::array<std::uint8_t,7> tags{0x83U,0x83U,0x03U,0x08U,0x03U,0x83U,0x03U};
        for(std::size_t index=0;index<tags.size();++index)
            scalar(tags[index],static_cast<std::uint32_t>(index+1U));
        scalar(0x02U,0x80000000U);
        const auto record=FirstCutListComponentReader::read(payload);
        check(record.controls==std::array<std::uint32_t,7>{1U,2U,3U,4U,5U,6U,7U} &&
                  std::bit_cast<std::uint32_t>(record.final_value)==0x80000000U,
              "first-cut list component reader retains its fixed controls and signed-zero scalar");
        check_rejected([&] {
            auto malformed=payload;
            malformed[10]=std::byte{0x04};
            static_cast<void>(FirstCutListComponentReader::read(malformed));
        }, "first-cut list component reader rejects a wrong control tag");
        check_rejected([&] {
            auto malformed=payload;
            malformed.push_back(std::byte{0});
            static_cast<void>(FirstCutListComponentReader::read(malformed));
        }, "first-cut list component reader rejects trailing bytes");
    }
    {
        using off::data::FirstCutOwnerReader;
        using off::data::DeferredOwnerReaderResult;
        std::array<std::byte,171> block{};
        block[0]=std::byte{0xab};
        block[4]=std::byte{0x89};
        block[5]=std::byte{0x08};
        block[13]=std::byte{0x06};
        // Six attachment delimiters, followed by the reviewed terminal.
        for(std::size_t index=0;index<6U;++index)
          block[14U+index*2U]=std::byte{0x06};
        block[170]=std::byte{0xff};
        const auto result=FirstCutOwnerReader::read(block);
        check(result.component_suffix.data()==block.data()+14U &&
                  result.component_suffix.size()==157U && result.component_extent==157U,
              "first-cut owner reader exposes only its reviewed bounded component suffix");
        check_rejected([&] {
            auto malformed=block;
            malformed[13]=std::byte{0x07};
            static_cast<void>(FirstCutOwnerReader::read(malformed));
        }, "first-cut owner reader rejects a malformed owner-base delimiter");
        check_rejected([&] {
            auto malformed=block;
            malformed[170]=std::byte{0x00};
            static_cast<void>(FirstCutOwnerReader::read(malformed));
        }, "first-cut owner reader rejects a missing terminal");
    }
    {
        using off::data::DeferredComponentReader;
        using off::data::DeferredOwnerReaderResult;
        using off::data::DeferredReaderSession;
        using off::data::DeferredReaderSessionState;
        using off::data::DeferredReaderWorkIdentity;

        constexpr DeferredReaderWorkIdentity identity{0x441U, 0x1234U, 17U};
        std::array owner_block{
            std::byte{0xa5},
            std::byte{0x03}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{0x06}, std::byte{0xff}, std::byte{0xee},
        };
        DeferredReaderSession session(identity, owner_block);
        owner_block[1] = std::byte{0x04};
        session.prepare(identity);
        bool copied_input = false;
        session.read_owner([&](std::span<const std::byte> copied) {
            copied_input = copied.size() == 9U && copied[1] == std::byte{0x03};
            return DeferredOwnerReaderResult{copied.subspan(1U), 7U};
        });
        std::size_t reader_input_size = 0U;
        const std::array<DeferredComponentReader, 1> readers{
            [&](std::span<const std::byte>& input) { reader_input_size = input.size(); },
        };
        const auto dispatched = session.read_components(readers);
        check(copied_input && reader_input_size == 5U && dispatched.dispatched_components == 1U &&
                  dispatched.continuation.size() == 1U && dispatched.continuation.front() == std::byte{0xff} &&
                  session.state() == DeferredReaderSessionState::component_read,
              "deferred reader session copies one owner block and dispatches only its explicit component extent");
        session.deactivate();
        check(session.state() == DeferredReaderSessionState::deactivated && session.owner_block().empty(),
              "deferred reader session deactivates and releases its copied owner block after component reading");

        check_rejected([&] { session.prepare(identity); },
                       "deferred reader session rejects reuse after deactivation");
        check_rejected([&] {
            const std::array<DeferredComponentReader, 1> one_reader{[](auto&) {}};
            static_cast<void>(session.read_components(one_reader));
        }, "deferred reader session rejects component reuse after deactivation");
        check_rejected([&] {
            DeferredReaderSession wrong_work(identity, owner_block);
            wrong_work.prepare({identity.resource, identity.source_offset + 1U, identity.source_directory_index});
        }, "deferred reader session requires the queued work identity at prepare");
        check_rejected([&] {
            DeferredReaderSession foreign_cursor(identity, owner_block);
            foreign_cursor.prepare(identity);
            const std::array foreign{std::byte{0x06}, std::byte{0xff}};
            foreign_cursor.read_owner([&](std::span<const std::byte>) {
                return DeferredOwnerReaderResult{foreign, foreign.size()};
            });
        }, "deferred reader session rejects a foreign owner-reader cursor");
        check_rejected([&] {
            DeferredReaderSession prefix_cursor(identity, owner_block);
            prefix_cursor.prepare(identity);
            prefix_cursor.read_owner([](std::span<const std::byte> copied) {
                return DeferredOwnerReaderResult{copied.first(2U), 2U};
            });
        }, "deferred reader session rejects an owner cursor that is not a suffix of its copied block");
        check_rejected([&] {
            DeferredReaderSession no_extent(identity, owner_block);
            no_extent.prepare(identity);
            no_extent.read_owner([](std::span<const std::byte> copied) {
                return DeferredOwnerReaderResult{copied, 0U};
            });
        }, "deferred reader session requires an owner-declared component extent");
        DeferredReaderSession failed_reader(identity, owner_block);
        failed_reader.prepare(identity);
        check_rejected([&] {
            failed_reader.read_owner([](std::span<const std::byte>) -> DeferredOwnerReaderResult {
                throw std::runtime_error("synthetic owner reader failure");
            });
        }, "deferred reader session propagates owner-reader failures");
        check(failed_reader.state() == DeferredReaderSessionState::deactivated,
              "deferred reader session permanently deactivates after an owner-reader failure");
        check_rejected([&] {
            const std::array<DeferredComponentReader, 0> no_readers{};
            static_cast<void>(failed_reader.read_components(no_readers));
        }, "deferred reader session rejects component work after a failed owner reader");
        DeferredReaderSession failed_component(identity, owner_block);
        failed_component.prepare(identity);
        failed_component.read_owner([](std::span<const std::byte> copied) {
            return DeferredOwnerReaderResult{copied.subspan(1U), 7U};
        });
        check_rejected([&] {
            const std::array<DeferredComponentReader, 1> throwing_reader{
                [](std::span<const std::byte>&) { throw std::runtime_error("synthetic component failure"); },
            };
            static_cast<void>(failed_component.read_components(throwing_reader));
        }, "deferred reader session propagates component-reader failures");
        check(failed_component.state() == DeferredReaderSessionState::deactivated,
              "deferred reader session permanently deactivates after a component-reader failure");
    }
    {
        using off::data::ComponentReaderContext;
        using off::data::ComponentReaderIdentity;
        using off::data::ComponentReaderInput;
        using off::data::ComponentReaderInvocationResult;
        using off::data::ComponentReaderKeysChild;

        constexpr ComponentReaderIdentity requested{0x42U, 3U};
        const int opaque_input = 17;
        const ComponentReaderInput input(&opaque_input);
        bool resolved = false;
        bool called = false;
        const auto absent = ComponentReaderContext::invoke_required_keys(
            requested, input,
            [&](std::uint64_t owner, std::array<char, 4> key)
                -> std::optional<ComponentReaderKeysChild> {
                resolved = owner == requested.owner && key == std::array<char, 4>{'K', 'E', 'Y', 'S'};
                return std::nullopt;
            },
            [&](const ComponentReaderInput&, std::uint64_t) { called = true; });
        check(absent == ComponentReaderInvocationResult::absent && resolved && !called,
              "component reader context queries the exact owner attachment and skips absent KEYS");

        const auto wrong_owner = ComponentReaderContext::invoke_required_keys(
            requested, input,
            [](std::uint64_t, std::array<char, 4>) -> std::optional<ComponentReaderKeysChild> {
                return ComponentReaderKeysChild{0x43U, 0x500U};
            },
            [&](const ComponentReaderInput&, std::uint64_t) { called = true; });
        check(wrong_owner == ComponentReaderInvocationResult::wrong_owner && !called,
              "component reader context rejects a KEYS child from another owner before reading");

        const auto unbound = ComponentReaderContext::invoke_required_keys(
            requested, input,
            [](std::uint64_t owner, std::array<char, 4>) -> std::optional<ComponentReaderKeysChild> {
                return ComponentReaderKeysChild{owner, 0U};
            },
            [&](const ComponentReaderInput&, std::uint64_t) { called = true; });
        check(unbound == ComponentReaderInvocationResult::unbound && !called,
              "component reader context rejects an unbound KEYS handle before reading");

        const auto invoked = ComponentReaderContext::invoke_required_keys(
            requested, input,
            [](std::uint64_t owner, std::array<char, 4>) -> std::optional<ComponentReaderKeysChild> {
                return ComponentReaderKeysChild{owner, 0x501U};
            },
            [&](const ComponentReaderInput& received, std::uint64_t handle) {
                called = received.opaque_context() == &opaque_input && handle == 0x501U;
            });
        check(invoked == ComponentReaderInvocationResult::invoked && called,
              "component reader context preserves opaque input separately from the KEYS handle");
    }
    {
        using off::data::OwnerAuxiliaryPropertyBlock;
        using off::data::OwnerBufKeysProfileParser;
        using off::data::KeysPropertyMaterializer;
        using off::data::SceneLifetimeKeysChildMapping;

        const auto write_word = [](std::span<std::byte> bytes, std::size_t offset,
                                   std::uint32_t value) {
            bytes[offset] = static_cast<std::byte>(value & 0xffU);
            bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
            bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
            bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xffU);
        };
        const auto supported_profile = [&] {
            std::array<std::byte, 64> bytes{};
            write_word(bytes, 4U, 0x80000040U);
            write_word(bytes, 8U, 64U);
            write_word(bytes, 12U, 1U);
            write_word(bytes, 16U, 0x5359454bU);
            write_word(bytes, 20U, 48U);
            return bytes;
        };

        auto bytes = supported_profile();
        const OwnerAuxiliaryPropertyBlock property{0x42U, 0x120U, bytes};
        const auto keys = OwnerBufKeysProfileParser::parse(property);
        check(keys.buf_auxiliary_offset == property.buf_auxiliary_offset &&
                  keys.name == std::array<char, 4>{'K', 'E', 'Y', 'S'} &&
                  keys.declared_extent == 48U && keys.bytes.data() == bytes.data() + 16U &&
                  keys.bytes.size() == 48U,
              "owner BUF KEYS profile exposes only the exact bounded child view");
        const std::array children{keys};
        const auto materialized = KeysPropertyMaterializer::materialize(
            property, children,
            [](const OwnerAuxiliaryPropertyBlock& requested_property,
               const auto& requested_child) -> std::optional<SceneLifetimeKeysChildMapping> {
                return SceneLifetimeKeysChildMapping{requested_property.owner,
                                                      requested_property.buf_auxiliary_offset,
                                                      requested_child.name,
                                                      requested_child.declared_extent, 0x700U};
            });
        check(materialized && materialized->opaque_handle == 0x700U,
              "owner BUF KEYS profile feeds the existing bounded descriptor materializer");

        check_rejected([&] {
            std::array<std::byte, 63> short_bytes{};
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, short_bytes}));
        }, "owner BUF KEYS profile rejects every outer boundary other than 64 bytes");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 0U, 1U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects a nonzero first word");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 4U, 64U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile requires the recovered high-bit outer tag");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 4U, 0x8000003fU);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects a tagged extent with the wrong low thirty bits");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 4U, 0xc0000040U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects an unobserved second tag bit");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 8U, 63U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects the wrong second outer extent");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 12U, 0U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects the wrong outer kind");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 16U, 0x454b4559U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects a child other than little-endian KEYS");
        check_rejected([&] {
            auto malformed = supported_profile();
            write_word(malformed, 20U, 47U);
            static_cast<void>(OwnerBufKeysProfileParser::parse({0x42U, 0x120U, malformed}));
        }, "owner BUF KEYS profile rejects a child extent that leaves trailing bytes");
    }
    {
        using off::data::KeysPropertyMaterializer;
        using off::data::OwnerAuxiliaryPropertyBlock;
        using off::data::OwnerAuxiliaryPropertyChild;
        using off::data::SceneLifetimeKeysChildMapping;

        std::array<std::byte, 64> property_bytes{};
        const OwnerAuxiliaryPropertyBlock property{0x42U, 0x120U, property_bytes};
        const OwnerAuxiliaryPropertyChild keys{0x120U, {'K', 'E', 'Y', 'S'}, 48U,
                                               std::span(property_bytes).subspan(16U, 48U)};
        const std::array children{keys};
        const auto materialized = KeysPropertyMaterializer::materialize(
            property, children,
            [](const OwnerAuxiliaryPropertyBlock& requested_property,
               const OwnerAuxiliaryPropertyChild& requested_child)
                -> std::optional<SceneLifetimeKeysChildMapping> {
                return SceneLifetimeKeysChildMapping{
                    requested_property.owner, requested_property.buf_auxiliary_offset,
                    requested_child.name, requested_child.declared_extent, 0x700U};
            });
        check(materialized && materialized->owner == 0x42U &&
                  materialized->buf_auxiliary_offset == 0x120U && materialized->opaque_handle == 0x700U,
              "KEYS materializer joins the owner-local BUF child to a supplied scene-lifetime handle");

        check_rejected([&] {
            const std::array duplicate{keys, keys};
            static_cast<void>(KeysPropertyMaterializer::materialize(property, duplicate, {}));
        }, "KEYS materializer rejects duplicate owner-local children");
        check_rejected([&] {
            const OwnerAuxiliaryPropertyChild missing{0x120U, {'N', 'O', 'P', 'E'}, 48U,
                                                       std::span(property_bytes).subspan(16U, 48U)};
            const std::array children_without_keys{missing};
            static_cast<void>(KeysPropertyMaterializer::materialize(property, children_without_keys, {}));
        }, "KEYS materializer rejects a missing KEYS child");
        check_rejected([&] {
            const OwnerAuxiliaryPropertyChild malformed{0x120U, {'K', 'E', 'Y', 'S'}, 47U,
                                                         std::span(property_bytes).subspan(16U, 47U)};
            const std::array malformed_children{malformed};
            static_cast<void>(KeysPropertyMaterializer::materialize(property, malformed_children, {}));
        }, "KEYS materializer rejects a malformed declared extent");
        const auto no_mapping = KeysPropertyMaterializer::materialize(property, children, {});
        const auto stale_mapping = KeysPropertyMaterializer::materialize(
            property, children,
            [](const OwnerAuxiliaryPropertyBlock&, const OwnerAuxiliaryPropertyChild&)
                -> std::optional<SceneLifetimeKeysChildMapping> {
                return SceneLifetimeKeysChildMapping{0x42U, 0x120U, {'K', 'E', 'Y', 'S'}, 48U, 0U};
            });
        check(!no_mapping && !stale_mapping,
              "KEYS materializer never synthesizes a child handle without a valid scene-lifetime mapping");
    }
    {
        using off::data::ComponentReaderContext;
        using off::data::ComponentReaderIdentity;
        using off::data::ComponentReaderInput;
        using off::data::KeysPropertyMaterializer;
        using off::data::OwnerAuxiliaryPropertyBlock;
        using off::data::OwnerBufKeysProfileParser;
        using off::data::SceneLifetimeKeysRegistry;
        using off::data::SceneLifetimeKeysRegistryInput;

        const auto write_word = [](std::span<std::byte> bytes, std::size_t offset,
                                   std::uint32_t value) {
            bytes[offset] = static_cast<std::byte>(value & 0xffU);
            bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
            bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
            bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xffU);
        };
        const auto supported_property = [&] {
            std::array<std::byte, 64> bytes{};
            write_word(bytes, 4U, 0x80000040U);
            write_word(bytes, 8U, 64U);
            write_word(bytes, 12U, 1U);
            write_word(bytes, 16U, 0x5359454bU);
            write_word(bytes, 20U, 48U);
            return bytes;
        };
        auto first_bytes = supported_property();
        auto second_bytes = supported_property();
        const OwnerAuxiliaryPropertyBlock first{0x42U, 0x120U, first_bytes};
        const OwnerAuxiliaryPropertyBlock second{0x43U, 0x140U, second_bytes};
        const std::array inputs{SceneLifetimeKeysRegistryInput{first, 0x700U},
                                SceneLifetimeKeysRegistryInput{second, 0x701U}};
        const auto registry = SceneLifetimeKeysRegistry::construct(inputs);
        const auto child = OwnerBufKeysProfileParser::parse(first);
        const std::array children{child};
        const auto materialized = KeysPropertyMaterializer::materialize(
            first, children, registry.materializer_resolver());
        bool reader_called = false;
        const int opaque_input = 7;
        const auto invoked = ComponentReaderContext::invoke_required_keys(
            ComponentReaderIdentity{first.owner, 0U}, ComponentReaderInput(&opaque_input),
            registry.component_reader_resolver(),
            [&](const ComponentReaderInput& input, std::uint64_t handle) {
                reader_called = input.opaque_context() == &opaque_input && handle == 0x700U;
            });
        check(registry.size() == 2U && materialized && materialized->opaque_handle == 0x700U &&
                  invoked == off::data::ComponentReaderInvocationResult::invoked && reader_called,
              "scene KEYS registry binds canonical owner-local properties to existing scene handles");

        const auto second_child = OwnerBufKeysProfileParser::parse(second);
        check(!registry.resolve(first, second_child) &&
                  !registry.resolve_required_keys(0x44U, {'K', 'E', 'Y', 'S'}),
              "scene KEYS registry rejects cross-owner and missing-owner lookups");

        first_bytes[0] = std::byte{1};
        const OwnerAuxiliaryPropertyBlock stale_first{0x42U, 0x120U, first_bytes};
        auto pristine_bytes = supported_property();
        const OwnerAuxiliaryPropertyBlock pristine_first{0x42U, 0x120U, pristine_bytes};
        const auto stale_child = OwnerBufKeysProfileParser::parse(pristine_first);
        check(!registry.resolve(stale_first, stale_child),
              "scene KEYS registry rejects a stale property source after construction");

        check_rejected([&] {
            const std::array duplicate{SceneLifetimeKeysRegistryInput{first, 0x702U},
                                       SceneLifetimeKeysRegistryInput{first, 0x703U}};
            static_cast<void>(SceneLifetimeKeysRegistry::construct(duplicate));
        }, "scene KEYS registry preflights duplicate owner-offset-key identities");
        check_rejected([&] {
            const std::array duplicate_handle{SceneLifetimeKeysRegistryInput{first, 0x702U},
                                              SceneLifetimeKeysRegistryInput{second, 0x702U}};
            static_cast<void>(SceneLifetimeKeysRegistry::construct(duplicate_handle));
        }, "scene KEYS registry preflights reused opaque handles");
        check_rejected([&] {
            const OwnerAuxiliaryPropertyBlock same_owner{0x42U, 0x121U, second_bytes};
            const std::array ambiguous_owner{SceneLifetimeKeysRegistryInput{first, 0x702U},
                                             SceneLifetimeKeysRegistryInput{same_owner, 0x703U}};
            static_cast<void>(SceneLifetimeKeysRegistry::construct(ambiguous_owner));
        }, "scene KEYS registry rejects ambiguous KEYS children for one component owner");
        check_rejected([&] {
            const std::array zero_handle{SceneLifetimeKeysRegistryInput{first, 0U}};
            static_cast<void>(SceneLifetimeKeysRegistry::construct(zero_handle));
        }, "scene KEYS registry rejects stale or zero scene handles");
    }
    {
        using off::data::KeysDescriptorBackingRequest;
        using off::data::KeysDescriptorRangeBinder;
        using off::data::MaterializedOwnerKeysChild;
        using off::data::OwnerAuxiliaryPropertyChild;

        const auto write_word = [](std::span<std::byte> bytes, std::size_t offset,
                                   std::uint32_t value) {
            bytes[offset] = static_cast<std::byte>(value & 0xffU);
            bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
            bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
            bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xffU);
        };
        const auto supported_child = [&] {
            std::array<std::byte, 48> bytes{};
            write_word(bytes, 0U, 0x5359454bU);
            write_word(bytes, 4U, 48U);
            write_word(bytes, 8U, 5U);
            write_word(bytes, 12U, static_cast<std::uint32_t>(-2));
            write_word(bytes, 16U, 2U);
            write_word(bytes, 20U, std::bit_cast<std::uint32_t>(0.25F));
            write_word(bytes, 24U, 0x10U);
            write_word(bytes, 28U, 0x20U);
            write_word(bytes, 32U, 0x30U);
            write_word(bytes, 36U, 0x40U);
            write_word(bytes, 40U, 0x50U);
            write_word(bytes, 44U, 0x60U);
            return bytes;
        };

        auto bytes = supported_child();
        const OwnerAuxiliaryPropertyChild child{0x120U, {'K', 'E', 'Y', 'S'}, 48U, bytes};
        const MaterializedOwnerKeysChild materialized{0x42U, 0x120U, 0x700U};
        bool request_seen = false;
        const auto bound = KeysDescriptorRangeBinder::bind(
            materialized, child, [&](const KeysDescriptorBackingRequest& request) {
                request_seen = request.owner == 0x42U && request.buf_auxiliary_offset == 0x120U &&
                               request.opaque_handle == 0x700U && request.descriptor.count_like == 5U &&
                               request.descriptor.inclusive_first == -2 &&
                               request.descriptor.inclusive_last == 2 &&
                               request.descriptor.spacing_or_rate == 0.25F &&
                               request.descriptor.first_offsets == std::array{0x10U, 0x20U, 0x30U} &&
                               request.descriptor.second_offsets == std::array{0x40U, 0x50U, 0x60U};
                return true;
            });
        check(bound && request_seen && bound->opaque_handle == 0x700U,
              "KEYS descriptor preserves fixed words and requires explicit backing availability");

        check(!KeysDescriptorRangeBinder::bind(materialized, child, {}) &&
                  !KeysDescriptorRangeBinder::bind(materialized, child,
                    [](const KeysDescriptorBackingRequest&) { return false; }),
              "KEYS descriptor does not synthesize unavailable backing");
        check_rejected([&] {
            auto malformed = bytes;
            write_word(malformed, 16U, static_cast<std::uint32_t>(-3));
            static_cast<void>(KeysDescriptorRangeBinder::bind(
                materialized, {0x120U, {'K', 'E', 'Y', 'S'}, 48U, malformed},
                [](const KeysDescriptorBackingRequest&) { return true; }));
        }, "KEYS descriptor rejects reversed signed inclusive bounds");
        check_rejected([&] {
            auto malformed = bytes;
            write_word(malformed, 8U, 4U);
            static_cast<void>(KeysDescriptorRangeBinder::bind(
                materialized, {0x120U, {'K', 'E', 'Y', 'S'}, 48U, malformed},
                [](const KeysDescriptorBackingRequest&) { return true; }));
        }, "KEYS descriptor rejects an inclusive range beyond its count-like word");
        check_rejected([&] {
            auto malformed = bytes;
            write_word(malformed, 20U, 0x7f800000U);
            static_cast<void>(KeysDescriptorRangeBinder::bind(
                materialized, {0x120U, {'K', 'E', 'Y', 'S'}, 48U, malformed},
                [](const KeysDescriptorBackingRequest&) { return true; }));
        }, "KEYS descriptor rejects non-finite spacing or rate");
    }
    {
        using off::data::BoundKeysDescriptorRange;
        using off::data::ImmutableKeysBackingView;
        using off::data::KeysBackingEvaluator;
        using off::data::KeysDescriptorRange;

        const auto set_i16 = [](std::vector<std::byte>& bytes, std::size_t offset,
                                std::int16_t value) {
            const auto bits = static_cast<std::uint16_t>(value);
            bytes[offset] = static_cast<std::byte>(bits & 0xffU);
            bytes[offset + 1U] = static_cast<std::byte>((bits >> 8U) & 0xffU);
        };
        std::vector<std::byte> backing(80U);
        // Each packed stream uses width two. The first stream selects 0, 1,
        // 1 for the three samples; the second maps its sole lookup to 1.
        backing[0] = std::byte{2}; backing[1] = std::byte{0x14};
        backing[8] = std::byte{2}; backing[9] = std::byte{0x40};
        backing[32] = std::byte{2}; backing[33] = std::byte{0x14};
        backing[40] = std::byte{2}; backing[41] = std::byte{0x40};
        const std::array<std::int16_t, 8> packed_values{-10, 10, 20, -20, 10, 30, 40, -40};
        for (std::size_t i = 0; i < packed_values.size(); ++i) {
            set_i16(backing, 16U + i * 2U, packed_values[i]);
        }
        const std::array<float, 6> floating_values{1.0F, 2.0F, 3.0F, 5.0F, 6.0F, 7.0F};
        for (std::size_t i = 0; i < floating_values.size(); ++i) {
            set_f32(backing, 48U + i * 4U, floating_values[i]);
        }
        const auto view = ImmutableKeysBackingView::create(0x1234U, backing);
        const BoundKeysDescriptorRange bound{
            .owner = 0x42U,
            .buf_auxiliary_offset = 0x120U,
            .opaque_handle = 0x700U,
            .descriptor = KeysDescriptorRange{
                .count_like = 3U,
                .inclusive_first = 0,
                .inclusive_last = 2,
                .spacing_or_rate = 1.0F,
                .first_offsets = {0U, 8U, 16U},
                .second_offsets = {32U, 40U, 48U},
            },
        };
        check(view && view->bytes().size() == backing.size() &&
                  view->bytes()[0U] == std::byte{2} && view->bytes()[1U] == std::byte{0x14} &&
                  view->bytes()[16U] == std::byte{0xf6} && view->bytes()[17U] == std::byte{0xff} &&
                  view->bytes()[48U] == std::byte{0},
              "KEYS backing view captures the complete source allocation before evaluation");
        backing[16] = std::byte{0}; // the view must retain an immutable copy.
        check(view && view->bytes().size() == backing.size() &&
                  view->bytes()[0U] == std::byte{2} && view->bytes()[1U] == std::byte{0x14} &&
                  view->bytes()[16U] == std::byte{0xf6} && view->bytes()[17U] == std::byte{0xff} &&
                  view->bytes()[48U] == std::byte{0},
              "KEYS backing view retains its snapshot after the caller mutates the source");
        auto first_only = bound;
        first_only.descriptor.count_like = 1U;
        const auto first_direct = view ? KeysBackingEvaluator::evaluate(*view, first_only, 0.0F) : std::nullopt;
        check(first_direct.has_value(),
              "KEYS backing evaluator produces sample zero without a successor");
        if(first_direct) {
            check(first_direct->first_group == std::array<float, 4>{-10.0F, 10.0F, 20.0F, -20.0F},
                  "KEYS backing evaluator reads sample-zero first group without a successor");
            check(first_direct->second_group == std::array<float, 3>{1.0F, 2.0F, 3.0F},
                  "KEYS backing evaluator reads sample-zero second group without a successor");
        }
        auto first_two = bound;
        first_two.descriptor.count_like = 2U;
        const auto second_direct = view ? KeysBackingEvaluator::evaluate(*view, first_two, 1.0F) : std::nullopt;
        check(second_direct &&
                  second_direct->first_group == std::array<float, 4>{10.0F, 30.0F, 40.0F, -40.0F} &&
                  second_direct->second_group == std::array<float, 3>{5.0F, 6.0F, 7.0F},
              "KEYS backing evaluator reads sample one without interpolation");
        const auto initial = view ? KeysBackingEvaluator::evaluate(*view, bound, 0.0F) : std::nullopt;
        check(initial && initial->first_group == std::array<float, 4>{-10.0F, 10.0F, 20.0F, -20.0F},
              "KEYS backing evaluator retains its immutable first sample after the caller mutates the source");
        const auto middle = view ? KeysBackingEvaluator::evaluate(*view, bound, 0.25F) : std::nullopt;
        if (!middle) {
            check(false, "KEYS backing evaluator did not produce the bounded interpolation sample");
        } else {
            check(middle->first_group == std::array<float, 4>{0.0F, 20.0F, 30.0F, -30.0F},
                  "KEYS backing evaluator produced an unexpected interpolated first group");
            check(middle->second_group == std::array<float, 3>{3.0F, 4.0F, 5.0F},
                  "KEYS backing evaluator produced an unexpected interpolated second group");
        }
        const auto final = view ? KeysBackingEvaluator::evaluate(*view, bound, 1.0F) : std::nullopt;
        check(final && final->first_group == std::array<float, 4>{10.0F, 30.0F, 40.0F, -40.0F} &&
                  final->second_group == std::array<float, 3>{5.0F, 6.0F, 7.0F},
              "KEYS backing evaluator uses the final sample directly without reading or blending a successor");
        check(view && !KeysBackingEvaluator::evaluate(*view, bound, -0.1F) &&
                  !KeysBackingEvaluator::evaluate(*view, bound, std::numeric_limits<float>::infinity()) &&
                  !ImmutableKeysBackingView::create(0U, backing),
              "KEYS backing evaluator requires a finite normalized coordinate and a live generation");
        auto malformed = ImmutableKeysBackingView::create(0x1234U, std::span<const std::byte>(backing).first(18U));
        check(malformed && !KeysBackingEvaluator::evaluate(*malformed, bound, 0.0F),
              "KEYS backing evaluator rejects incomplete final tables instead of manufacturing a sample");
        auto width = backing;
        width[0] = std::byte{25};
        const auto invalid_width = ImmutableKeysBackingView::create(0x1234U, width);
        check(invalid_width && !KeysBackingEvaluator::evaluate(*invalid_width, bound, 0.0F),
              "KEYS backing evaluator rejects unsupported packed widths");

        auto unaligned_wide = backing;
        std::fill(unaligned_wide.begin(), unaligned_wide.end(), std::byte{0});
        unaligned_wide[0] = std::byte{23};
        const auto unaligned_wide_view = ImmutableKeysBackingView::create(0x1234U, unaligned_wide);
        auto two_samples = bound;
        two_samples.descriptor.count_like = 2U;
        check(unaligned_wide_view && !KeysBackingEvaluator::evaluate(*unaligned_wide_view, two_samples, 0.0F),
              "KEYS backing evaluator rejects a 23-bit field that exceeds the recovered three-byte window");

        const std::array<std::byte, 3> truncated_stream{
            std::byte{2}, std::byte{0}, std::byte{0}};
        const auto truncated_stream_view = ImmutableKeysBackingView::create(0x1234U, truncated_stream);
        check(truncated_stream_view &&
                  !KeysBackingEvaluator::evaluate(*truncated_stream_view, bound, 0.0F),
              "KEYS backing evaluator rejects a packed stream without its recovered three-byte window");
    }
    {
        using off::data::KeysBackingSample;
        using off::data::MatPosPoseEvaluator;

        const auto orientation = MatPosPoseEvaluator::evaluate({
            .first_group = {32512.0F, -32512.0F, 32512.0F, -32512.0F},
            .second_group = {4.0F, -5.0F, 6.0F},
        });
        check(orientation && orientation->normalized_orientation == std::array<float, 4>{0.5F, -0.5F, 0.5F, -0.5F} &&
                  orientation->translation == std::array<float, 3>{4.0F, -5.0F, 6.0F},
              "MatPos pose evaluator normalizes quantized orientation without reordering it and preserves translation");

        const auto identity = MatPosPoseEvaluator::evaluate({
            .first_group = {0.0F, 0.0F, 0.0F, 32512.0F},
            .second_group = {},
        });
        check(identity && identity->basis == std::array<float, 9>{1.0F, 0.0F, 0.0F,
                                                                   0.0F, 1.0F, 0.0F,
                                                                   0.0F, 0.0F, 1.0F},
              "MatPos pose evaluator converts an identity x-y-z-w quaternion to the logical row-major identity basis");

        constexpr float quarter_turn_component = 22989.0F;
        const auto x_quarter_turn = MatPosPoseEvaluator::evaluate({
            .first_group = {quarter_turn_component, 0.0F, 0.0F, quarter_turn_component},
            .second_group = {},
        });
        constexpr std::array<float, 9> x_quarter_turn_basis{1.0F, 0.0F, 0.0F,
                                                              0.0F, 0.0F, 1.0F,
                                                              0.0F, -1.0F, 0.0F};
        check(x_quarter_turn && std::equal(x_quarter_turn->basis.begin(), x_quarter_turn->basis.end(),
                                            x_quarter_turn_basis.begin(), [](float actual, float expected) {
                                                return std::abs(actual - expected) < 0.000001F;
                                            }),
              "MatPos pose evaluator uses the recovered row-major quaternion basis signs and layout");

        const auto arbitrary_scale = MatPosPoseEvaluator::evaluate({
            .first_group = {65024.0F, 0.0F, 0.0F, 0.0F},
            .second_group = {},
        });
        check(arbitrary_scale && arbitrary_scale->normalized_orientation == std::array<float, 4>{1.0F, 0.0F, 0.0F, 0.0F},
              "MatPos pose evaluator scales before normalizing instead of assuming a fixed input magnitude");

        check(!MatPosPoseEvaluator::evaluate({.first_group = {}, .second_group = {}}) &&
                  !MatPosPoseEvaluator::evaluate({
                      .first_group = {0.0F, 0.0F, 0.0F, 32512.0F},
                      .second_group = {std::numeric_limits<float>::infinity(), 0.0F, 0.0F},
                  }),
              "MatPos pose evaluator rejects degenerate or non-finite detached samples");
    }
    {
        using off::data::TypedValue;
        using off::data::TypedValueCursor;
        using off::data::TypedValueKind;
        constexpr std::array values{
            TypedValue{TypedValueKind::scalar, 0x80000000U, 2U},
            TypedValue{TypedValueKind::integer, 0xffffffffU, 3U},
            TypedValue{TypedValueKind::continuation, 0U, 3U},
            TypedValue{TypedValueKind::opaque_reference, 0x80000005U, 3U},
        };
        TypedValueCursor cursor(values);
        cursor.require_next_schema_class(2U);
        check(cursor.peek() != nullptr && cursor.peek()->kind == TypedValueKind::scalar &&
                  cursor.next_schema_class() == 2U &&
                  cursor.remaining() == values.size(),
              "typed cursor peeks without advancing its bounded token input");
        check(std::bit_cast<std::uint32_t>(cursor.scalar()) == 0x80000000U &&
                  cursor.integer() == -1,
              "typed cursor preserves scalar bits and decodes signed integer values");
        cursor.continuation();
        check(cursor.opaque_reference() == 0x80000005U && cursor.empty(),
              "typed cursor retains opaque references and explicit continuation markers");
        cursor.finish();
        check_rejected([&] { cursor.continuation(); },
                       "typed cursor rejects reads beyond its supplied token boundary");
        check_rejected([&] { cursor.require_next_schema_class(2U); },
                       "typed cursor rejects an incompatible next schema class");
        TypedValueCursor wrong_kind(values);
        check_rejected([&] { static_cast<void>(wrong_kind.opaque_reference()); },
                       "typed cursor rejects a mismatched token kind without advancing");
        check(wrong_kind.remaining() == values.size(),
              "typed cursor keeps position after a rejected token-kind read");
    }
    intro_window_tests();
    startloader_load_screen_tests();
    intro_legal_picture_tests();
    intro_camera_tests();
    intro_fade_picture_tests();
    cut_tests();
    const auto decode_intro = [](std::vector<std::byte> bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(
            std::move(bytes))).intro_movie_controller_source(1);
    };
    for (unsigned int count = 0; count <= 2; ++count) {
        const auto decoded = decode_intro(intro_controller_fixture("", count));
        check(decoded.sequence_reference == 0xf1234567U &&
                  decoded.group_reference == 0x87654321U &&
                  decoded.additional_reference == 0xffffffffU &&
                  decoded.authored_option == 0x80000000U &&
                  decoded.destination.empty() &&
                  decoded.first_optional_reference.has_value() == (count >= 1U) &&
                  decoded.second_optional_reference.has_value() == (count == 2U),
              "decode raw intro fields and optional prefixes, ignoring external padding");
        if (count >= 1U) check(*decoded.first_optional_reference == 0U,
                             "preserve explicit zero optional reference");
        if (count == 2U) check(*decoded.second_optional_reference == 0xc1234567U,
                              "preserve optional reference high bits");
    }
    const std::string raw_destination(99U, static_cast<char>(0xfe));
    check(decode_intro(intro_controller_fixture(raw_destination)).destination == raw_destination,
          "preserve 99 raw non-ASCII destination bytes in owned output");
    check_rejected([&] { decode_intro(intro_controller_fixture(std::string(100U, 'x'))); },
                   "reject a 100-byte intro destination");
    const auto reject_intro_mutation = [&](auto mutate) {
        auto bytes = intro_controller_fixture("", 2);
        mutate(bytes);
        check_rejected([&] { decode_intro(std::move(bytes)); },
                       "reject malformed restricted intro controller source");
    };
    // Empty destination yields a 44-byte block: every declared truncation must
    // fail even though the untouched backing image still contains all fields.
    for (std::uint32_t size = 0; size < 44U; ++size) {
        reject_intro_mutation([&](auto& b) { set_u32(b, 609U, size); });
    }
    for (auto position : {4U, 9U, 10U, 15U, 20U, 25U, 27U, 32U, 37U, 42U, 43U}) {
        reject_intro_mutation([&](auto& b) { b[609U + position] = std::byte{0x45}; });
    }
    for (auto header : {0x0100002cU, 0x00ffffffU, 45U}) {
        reject_intro_mutation([&](auto& b) { set_u32(b, 609U, header); });
    }
    for (auto offset : {0U, 1023U}) {
        reject_intro_mutation([&](auto& b) { set_u32(b, 9U + 336U + 32U, offset); });
    }
    reject_intro_mutation([](auto& b) { set_u32(b, 9U + 336U + 16U, 0U); });
    reject_intro_mutation([](auto& b) { set_u32(b, 9U + 336U + 12U, 1U); });
    for (auto count : {0U, 2U}) {
        reject_intro_mutation([&](auto& b) { set_u32(b, 521U, count); });
    }
    for (auto parameter : {0x3f800000U, 0x7f800000U, 0x7fc00000U}) {
        reject_intro_mutation([&](auto& b) { set_u32(b, 529U, parameter); });
    }
    reject_intro_mutation([](auto& b) { b[553U] = std::byte{'X'}; });
    reject_intro_mutation([](auto& b) { b[570U] = std::byte{'X'}; });
    reject_intro_mutation([](auto& b) { set_u32(b, 525U, 1023U); });
    reject_intro_mutation([](auto& b) { set_u32(b, 614U, 8U); });
    reject_intro_mutation([](auto& b) { b[641U] = std::byte{0x08}; });
    reject_intro_mutation([](auto& b) { b[635U] = std::byte{1}; });
    reject_intro_mutation([](auto& b) { b[619U] = std::byte{0x08}; });
    reject_intro_mutation([](auto& b) {
        set_u32(b, 609U, 49U);
        b[651U] = std::byte{0x88};
        set_u32(b, 652U, 0U);
        b[656U] = std::byte{0x06};
        b[657U] = std::byte{0xff};
    });
    reject_intro_mutation([](auto& b) {
        // No destination terminator anywhere in the enclosing block.
        std::fill(b.begin() + 635U, b.begin() + 653U, std::byte{0x81});
    });
    auto negative_zero = intro_controller_fixture();
    set_u32(negative_zero, 529U, 0x80000000U);
    check(decode_intro(std::move(negative_zero)).destination == "SyntheticDestination",
          "accept finite negative-zero attachment parameter");
    check_rejected([&] {
        static_cast<void>(off::data::GmsImage::parse(off::data::PackedResource::parse(
            intro_controller_fixture())).intro_movie_controller_source(3));
    }, "reject out-of-range intro source index");
    const auto image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(packed_fixture())
    );
    check(!image.local_source_for_authored_reference(0U), "raw zero is a null authored reference");
    for (auto flag : {0U, 0x80000000U}) {
        check(image.local_source_for_authored_reference(flag | 1U) == 0U &&
                  image.local_source_for_authored_reference(flag | 3U) == 2U,
              "resolve first and last tagged and untagged source indices");
    }
    check(image.directory()[1].local_slot_index != 1U &&
              image.local_source_for_authored_reference(2U) == 1U,
          "authored references select source directory rather than reordered pool slots");
    for (auto raw : {4U, 0x80000004U, 0x80000000U, 0xffffffffU, 0x40000001U, 0xc0000001U}) {
        check_rejected([&] { static_cast<void>(image.local_source_for_authored_reference(raw)); },
                       "reject unresolved or out-of-range authored references without masking bit30");
    }
    const auto decode_list = [](const std::vector<std::byte>& bytes) {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(bytes))
            .intro_source_reference_list(1U);
    };
    for (const auto& words : std::vector<std::vector<std::uint32_t>>{
             {}, {1U}, {0U, 0x80000000U, 0xffffffffU, 0x40000001U, 1U, 1U}}) {
        check(decode_list(intro_list_fixture(words)) == words,
              "decode raw list count, order, duplicates and unresolved words ignoring external padding");
    }
    const auto reject_list_mutation = [&](auto mutate) {
        auto bytes = intro_list_fixture({1U, 2U});
        mutate(bytes);
        check_rejected([&] { static_cast<void>(decode_list(bytes)); },
                       "reject malformed restricted intro source-reference list");
    };
    for (std::uint32_t size = 0; size < 19U; ++size) {
        reject_list_mutation([&](auto& b) { set_u32(b, 609U, size); });
    }
    for (auto header : {20U, 0x01000013U, 0x00ffffffU}) {
        reject_list_mutation([&](auto& b) { set_u32(b, 609U, header); });
    }
    for (auto count : {0U, 3U, 5U, 11U, 13U, 16U, 0xfffffffcU, 0xffffffffU}) {
        reject_list_mutation([&](auto& b) { set_u32(b, 614U, count); });
    }
    for (auto offset : {613U, 626U, 627U}) {
        reject_list_mutation([&](auto& b) { b[offset] = std::byte{0}; });
    }
    reject_list_mutation([](auto& b) { b[613U] = std::byte{0x09}; });
    reject_list_mutation([](auto& b) { set_u32(b, 9U + 336U + 16U, 0U); });
    reject_list_mutation([](auto& b) { set_u32(b, 9U + 336U + 12U, 1U); });
    reject_list_mutation([](auto& b) { set_u32(b, 9U + 336U + 20U, 512U); });
    for (auto offset : {0U, 1023U}) {
        reject_list_mutation([&](auto& b) { set_u32(b, 9U + 336U + 32U, offset); });
    }
    check_rejected([&] { static_cast<void>(image.intro_source_reference_list(3U)); },
                   "reject out-of-range list directory index");
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
    {
        auto bytes = packed_fixture();
        constexpr std::size_t block_offset = 452;
        set_u32(bytes, 9 + 80 + 32, block_offset);
        set_u32(bytes, 9 + block_offset, 8U);
        bytes[9 + block_offset + 4] = std::byte{0xaa};
        bytes[9 + block_offset + 5] = std::byte{0xbb};
        bytes[9 + block_offset + 6] = std::byte{0xcc};
        bytes[9 + block_offset + 7] = std::byte{0xdd};
        const auto deferred_image = off::data::GmsImage::parse(
            off::data::PackedResource::parse(bytes)
        );
        const auto block = deferred_image.deferred_source_block(0);
        check(block.size() == 8U && block[0] == std::byte{8} &&
                  block[4] == std::byte{0xaa} && block[7] == std::byte{0xdd},
              "return the exact header-inclusive deferred source block");
        check_rejected(
            [&deferred_image] {
                static_cast<void>(deferred_image.deferred_source_block(1));
            },
            "reject a directory entry without deferred source data"
        );
        check_rejected(
            [&deferred_image] {
                static_cast<void>(deferred_image.deferred_source_block(3));
            },
            "reject an out-of-range deferred source directory index"
        );
    }
    check_rejected(
        [] {
            auto bytes = packed_fixture();
            set_u32(bytes, 9 + 80 + 32, 452);
            set_u32(bytes, 9 + 452, 0x01000004U);
            const auto deferred_image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(deferred_image.deferred_source_block(0));
        },
        "reject a deferred source block with nonzero header high byte"
    );
    check_rejected(
        [] {
            auto bytes = packed_fixture();
            set_u32(bytes, 9 + 80 + 32, 452);
            set_u32(bytes, 9 + 452, 3U);
            const auto deferred_image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(deferred_image.deferred_source_block(0));
        },
        "reject a deferred source block smaller than its header"
    );
    check_rejected(
        [] {
            auto bytes = packed_fixture();
            set_u32(bytes, 9 + 80 + 32, 508);
            const auto deferred_image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(deferred_image.deferred_source_block(0));
        },
        "reject a truncated deferred source block header"
    );
    check_rejected(
        [] {
            auto bytes = packed_fixture();
            set_u32(bytes, 9 + 80 + 32, 452);
            set_u32(bytes, 9 + 452, 61U);
            const auto deferred_image = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes)
            );
            static_cast<void>(deferred_image.deferred_source_block(0));
        },
        "reject a deferred source block extending beyond the image"
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
    {
        auto bytes = packed_fixture();
        constexpr std::size_t envelope = 9U;
        set_u32(bytes, envelope + 8U, 452U);
        set_u32(bytes, envelope + 16U, 480U);
        constexpr char label[] = "Global";
        std::copy_n(reinterpret_cast<const std::byte*>(label), sizeof(label),
                    bytes.begin() + envelope + 452U);
        bytes[envelope + 459U] = std::byte{8};
        bytes[envelope + 463U] = std::byte{0x31};
        set_u32(bytes, envelope + 480U, 2U);
        for (std::size_t index = 0; index < 24U; ++index)
            bytes[envelope + 484U + index] = static_cast<std::byte>(index + 1U);
        const auto outer_image = off::data::GmsImage::parse(
            off::data::PackedResource::parse(bytes));
        const auto sources = outer_image.outer_loader_sources();
        check(sources.named_global && sources.named_global->size() == 15U &&
                  (*sources.named_global)[0] == std::byte{'G'} &&
                  (*sources.named_global)[7] == std::byte{8} &&
                  (*sources.named_global)[11] == std::byte{0x31},
              "retain the exact declared source-owned named/global block");
        const auto envelope_value =
            off::graphics::parse_intro_named_global_section_envelope(
                *sources.named_global);
        check(envelope_value.label == "Global" &&
                  envelope_value.tagged_block.size() == 8U &&
                  envelope_value.tagged_block[4] == std::byte{0x31},
              "pass extracted named/global bytes through the tagged envelope boundary");
        check(sources.allocation_sizing_rows.size() == 2U &&
                  sources.allocation_sizing_rows[0][0] == 0x04030201U &&
                  sources.allocation_sizing_rows[1][2] == 0x18171615U,
              "decode the source-owned allocation-sizing rows");
    }
    {
        std::vector<std::byte> payload(176);
        set_u32(payload, 0U, 164U);
        set_u32(payload, 4U, 168U);
        set_u32(payload, 12U, 4U);
        set_u32(payload, 16U, 172U);
        set_u32(payload, 20U, 32U);
        set_u32(payload, 24U, 132U);
        set_u32(payload, 32U, 1U);
        set_u32(payload, 132U, 8U);
        for (std::size_t index = 0; index < 8U; ++index)
            payload[136U + index] = static_cast<std::byte>(0xa0U + index);
        set_u32(payload, 144U, 2U);
        set_u32(payload, 148U, 7U);
        set_u32(payload, 152U, 9U);
        set_u32(payload, 156U, 17U);
        set_u32(payload, 160U, 18U);
        std::vector<std::byte> bytes;
        append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
        append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9U));
        bytes.push_back(std::byte{1});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        const auto value = off::data::GmsImage::parse(
            off::data::PackedResource::parse(bytes));
        const auto sources = value.outer_loader_sources();
        check(sources.renderer_resource && sources.renderer_resource->size() == 8U &&
                  (*sources.renderer_resource)[0] == std::byte{0xa0} &&
                  (*sources.renderer_resource)[7] == std::byte{0xa7},
              "retain exactly the framed renderer-resource payload");
        check(sources.resource_associations.size() == 2U &&
                  sources.resource_associations[0] ==
                      std::array<std::uint32_t, 2>{7U, 9U} &&
                  sources.resource_associations[1] ==
                      std::array<std::uint32_t, 2>{17U, 18U},
              "decode ordered renderer-resource association pairs");
        check_rejected(
            [bytes] {
                auto malformed = bytes;
                set_u32(malformed, 9U + 132U, 100U);
                const auto image = off::data::GmsImage::parse(
                    off::data::PackedResource::parse(malformed));
                static_cast<void>(image.outer_loader_sources());
            },
            "reject a renderer-resource payload overlapping the directory");
        check_rejected(
            [bytes] {
                auto malformed = bytes;
                set_u32(malformed, 9U + 144U, 3U);
                const auto image = off::data::GmsImage::parse(
                    off::data::PackedResource::parse(malformed));
                static_cast<void>(image.outer_loader_sources());
            },
            "reject a resource-association count with a mismatched extent");
    }
    check_rejected(
        [] {
            auto bytes = packed_fixture();
            set_u32(bytes, 9U + 16U, 504U);
            set_u32(bytes, 9U + 504U, 1U);
            const auto value = off::data::GmsImage::parse(
                off::data::PackedResource::parse(bytes));
            static_cast<void>(value.outer_loader_sources());
        },
        "reject a truncated allocation-sizing table"
    );

    return failures == 0 ? 0 : 1;
}
