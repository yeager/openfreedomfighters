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
