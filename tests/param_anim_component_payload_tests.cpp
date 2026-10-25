#include "off/data/param_anim_component_payload.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using Bytes = std::vector<std::byte>;

void check(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

template<class Action>
void rejects(Action action, const char* message) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    check(false, message);
}

void append_u32(Bytes& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_i32(Bytes& bytes, std::uint8_t tag, std::int32_t value) {
    bytes.push_back(static_cast<std::byte>(tag));
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void append_f32(Bytes& bytes, float value) {
    bytes.push_back(std::byte{0x02});
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void append_f64(Bytes& bytes, double value) {
    bytes.push_back(std::byte{0x01});
    const auto bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned int shift = 0; shift < 64U; shift += 8U)
        bytes.push_back(static_cast<std::byte>((bits >> shift) & 0xffU));
}

void append_string(Bytes& bytes, const char* value) {
    bytes.push_back(std::byte{0x04});
    while (*value != '\0')
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(*value++)));
    bytes.push_back(std::byte{0});
}

void test_defaults_and_empty_boundaries() {
    using off::data::ParamAnimComponentPayloadReader;
    const auto empty = ParamAnimComponentPayloadReader::read({});
    check(empty.initial_control && empty.scalar_a == 0.0F && empty.scalar_b == -1.0F &&
              empty.mode_word == 0x0100U && !empty.control_a && empty.control_b &&
              empty.scalar_c == 25.0F && empty.consumed_bytes == 0U &&
              empty.consumed_values == 0U,
          "an empty component slice retains native reader defaults without admission");

    const Bytes terminal{std::byte{0xff}};
    const auto stopped = ParamAnimComponentPayloadReader::read(terminal);
    check(stopped.consumed_bytes == 0U && stopped.consumed_values == 0U &&
              stopped.scalar_c == 25.0F,
          "a terminal at the bounded slice start leaves defaults unchanged");
}

void test_optional_modes_and_component_extent() {
    using off::data::ParamAnimComponentPayloadReader;
    Bytes payload;
    append_i32(payload, 0x03, 1);       // initial control
    append_i32(payload, 0x83, 0);       // the first control is copied to control_a
    append_f64(payload, 12.5);          // scalar_c uses float32 after conversion
    append_f32(payload, 0.75F);          // integer conversion truncates toward zero
    append_i32(payload, 0x0a, 0x12345); // optional low-word mode
    append_f32(payload, 50.0F);          // optional scaled pair
    append_f32(payload, -25.0F);
    append_i32(payload, 0x03, 77);      // unrelated trailing value remains unread
    const auto result = ParamAnimComponentPayloadReader::read(payload);

    check(result.control_a && !result.control_b && !result.initial_control,
          "ordinary mode preserves the native control overwrite order");
    check(result.scalar_c == 12.5F && result.mode_word == 0x2345U &&
              result.scalar_a == 2.0F && result.scalar_b == -1.0F,
          "optional word and scaled float pair retain their typed conversions");
    check(result.consumed_values == 7U && result.consumed_bytes + 5U == payload.size(),
          "component reader reports its consumed prefix and leaves following values to its owner");
}

void test_type_two_branch_and_low_six_bit_tags() {
    using off::data::ParamAnimComponentPayloadReader;
    Bytes payload;
    append_i32(payload, 0x83, 1); // high tag flags do not change the numeric class
    append_f32(payload, 2.75F);    // class 2 suppresses the two control reads
    append_i32(payload, 0x03, 2);  // trailing control overwrites the initial state
    const auto result = ParamAnimComponentPayloadReader::read(payload);

    check(!result.control_a && result.control_b && result.initial_control &&
              result.scalar_c == 2.75F,
          "type-two branch leaves the first control value in place");
    check(result.consumed_values == 3U && result.consumed_bytes == payload.size(),
          "type-two branch consumes only its three source values");
}

void test_numeric_conversion_and_unread_nonnumeric_tail() {
    using off::data::ParamAnimComponentPayloadReader;
    Bytes payload;
    append_f64(payload, 1.9);       // integer conversion truncates toward zero
    append_i32(payload, 0x0b, -2);  // another accepted signed compact class
    append_i32(payload, 0x08, 40);
    append_f32(payload, -0.8F);
    append_string(payload, "tail");
    const auto result = ParamAnimComponentPayloadReader::read(payload);

    check(result.control_a && result.control_b && !result.initial_control &&
              result.scalar_c == 40.0F,
          "typed numeric conversions match the reader's truncation and signed rules");
    check(result.consumed_values == 4U && result.consumed_bytes + 6U == payload.size(),
          "the component reader does not consume an unrelated string tail");
}

void test_unsigned_integer_payload_float_conversion() {
    using off::data::ParamAnimComponentPayloadReader;
    struct Example { std::uint32_t payload; std::uint32_t expected_float_bits; };
    // Expected IEEE binary32 encodings are independent of a host conversion
    // expression, including ties on both sides of the unsigned high-bit range.
    constexpr Example examples[]{
        {0U, 0x00000000U},
        {0x01000001U, 0x4b800000U}, // 2^24 + 1 ties down to even
        {0x01000003U, 0x4b800002U}, // 2^24 + 3 ties up to even
        {0x80000000U, 0x4f000000U},
        {0x80000080U, 0x4f000000U}, // 2^31 + 128 ties down
        {0x80000081U, 0x4f000001U}, // one above the midpoint
        {0xffffff7fU, 0x4f7fffffU}, // one below the final midpoint
        {0xffffff80U, 0x4f800000U}, // final midpoint rounds to 2^32
        {0xffffffffU, 0x4f800000U},
    };
    constexpr std::uint8_t classes[]{3U, 8U, 10U, 11U};
    for (const auto cls : classes) {
        for (const auto example : examples) {
            Bytes payload;
            append_i32(payload, cls, -1);
            append_i32(payload, cls, 0);
            payload.push_back(static_cast<std::byte>(cls | 0xc0U));
            append_u32(payload, example.payload);
            append_i32(payload, cls, std::numeric_limits<std::int32_t>::min());
            append_i32(payload, 10U, -1);
            const auto value = ParamAnimComponentPayloadReader::read(payload);
            check(std::bit_cast<std::uint32_t>(value.scalar_c) == example.expected_float_bits,
                  "all integer classes convert unsigned payloads with binary32 rounding in float fields");
            check(value.control_a && !value.control_b && value.initial_control &&
                  value.mode_word == 0xffffU,
                  "negative and zero integer controls and signed low-word mode retain their behavior");
            check(value.consumed_values == 5U && value.consumed_bytes == payload.size(),
                  "unsigned float interpretation leaves field boundaries unchanged");
        }
    }
}

void test_length_class_advances_over_its_bounded_payload() {
    using off::data::ParamAnimComponentPayloadReader;
    for (const auto tag : {std::byte{0x07}, std::byte{0x09}}) {
    Bytes payload{tag};
    append_u32(payload, 7U); // four-byte extent word and three opaque bytes
    payload.insert(payload.end(), {std::byte{0xff}, std::byte{0x02}, std::byte{0x03}});
    append_f32(payload, 7.5F);
    append_i32(payload, 0x03, 1);

    const auto result = ParamAnimComponentPayloadReader::read(payload);
    check(!result.control_a && !result.control_b && result.scalar_c == 7.5F &&
              result.initial_control,
          "the length class converts to zero and advances over its opaque bounded body");
    check(result.consumed_values == 3U && result.consumed_bytes == payload.size(),
          "length-delimited bytes are not mistaken for additional compact values");

    Bytes numeric_zero;
    append_i32(numeric_zero, 3U, 1);
    append_i32(numeric_zero, 3U, 0);
    numeric_zero.push_back(tag);
    append_u32(numeric_zero, 6U);
    numeric_zero.insert(numeric_zero.end(), {std::byte{6}, std::byte{0xff}});
    append_string(numeric_zero, "zero");
    const auto zero = ParamAnimComponentPayloadReader::read(numeric_zero);
    check(zero.scalar_c == 0.F && !zero.initial_control &&
          zero.consumed_bytes == numeric_zero.size(),
          "length float and string integer fields yield zero after bounded advancement");

    Bytes truncated{tag};
    append_u32(truncated, 8U);
    truncated.insert(truncated.end(), {std::byte{0x00}, std::byte{0x00}, std::byte{0x00}});
    rejects([&] { (void)ParamAnimComponentPayloadReader::read(truncated); },
            "a length class extending beyond the component slice must be rejected");
    }
}

void test_class_twelve_is_framing_only_for_param_anim() {
    using off::data::ParamAnimComponentPayloadReader;
    for (const auto tag : {std::byte{0x0c}, std::byte{0x4c}, std::byte{0x8c}, std::byte{0xcc}}) {
        Bytes length{tag};
        append_u32(length, 6U);
        length.insert(length.end(), {std::byte{6}, std::byte{0xff}});
        // Check all four required typed positions on the long control branch.
        for (unsigned position{}; position<4U; ++position) {
            Bytes payload;
            for (unsigned field{}; field<4U; ++field) {
                if (field==position) payload.insert(payload.end(),length.begin(),length.end());
                else append_i32(payload,3U,1);
            }
            rejects([&] { (void)ParamAnimComponentPayloadReader::read(payload); },
                    "class twelve cannot satisfy a required ParamAnim field regardless of high tag bits");
        }
        Bytes payload;
        append_i32(payload,3U,1);append_f32(payload,8.F);append_i32(payload,3U,0);
        const auto prefix_size=payload.size();
        payload.insert(payload.end(),length.begin(),length.end());
        const auto result=ParamAnimComponentPayloadReader::read(payload);
        check(result.scalar_c==8.F && result.consumed_bytes==prefix_size &&
              result.consumed_values==3U,
              "ParamAnim leaves a class-twelve trailing value for generic framing dispatch");
    }
}

void test_x86_truncation_invalid_results() {
    using off::data::ParamAnimComponentPayloadReader;
    Bytes payload;
    append_f32(payload, std::numeric_limits<float>::quiet_NaN());
    append_f32(payload, 1.25F); // class 2 selects the short control branch
    append_f64(payload, 2147483648.0); // CVTTSD2SI returns its integer-indefinite value
    const auto result = ParamAnimComponentPayloadReader::read(payload);
    check(result.control_b && !result.control_a && result.initial_control &&
              result.scalar_c == 1.25F && result.consumed_values == 3U &&
              result.consumed_bytes == payload.size(),
          "x86 truncating conversions map NaN and positive overflow to INT_MIN before boolean tests");
}

void test_truncated_required_values_reject_without_partial_state() {
    using off::data::ParamAnimComponentPayloadReader;
    Bytes payload;
    append_i32(payload, 0x03, 1);
    rejects([&] { (void)ParamAnimComponentPayloadReader::read(payload); },
            "a truncated required control sequence must be rejected");

    Bytes truncated_float;
    append_i32(truncated_float, 0x03, 1);
    append_f32(truncated_float, 1.0F);
    append_i32(truncated_float, 0x03, 0);
    truncated_float.pop_back();
    rejects([&] { (void)ParamAnimComponentPayloadReader::read(truncated_float); },
            "a truncated typed payload must be rejected at its bounded boundary");
}

} // namespace

int main() {
    test_defaults_and_empty_boundaries();
    test_optional_modes_and_component_extent();
    test_type_two_branch_and_low_six_bit_tags();
    test_numeric_conversion_and_unread_nonnumeric_tail();
    test_unsigned_integer_payload_float_conversion();
    test_length_class_advances_over_its_bounded_payload();
    test_class_twelve_is_framing_only_for_param_anim();
    test_x86_truncation_invalid_results();
    test_truncated_required_values_reject_without_partial_state();
}
