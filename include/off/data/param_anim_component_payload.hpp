#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace off::data {

// Field names describe only the recovered reader roles. This is component
// state, not proof that an enclosing owner or reader lifecycle is complete.
struct ParamAnimComponentPayloadState final {
    bool initial_control{true};
    float scalar_a{0.0F};
    float scalar_b{-1.0F};
    std::uint16_t mode_word{0x0100U};
    bool control_a{false};
    bool control_b{true};
    float scalar_c{25.0F};
    std::size_t consumed_bytes{};
    std::size_t consumed_values{};
};

class ParamAnimComponentPayloadReader final {
public:
    // `input` is a bounded component slice ending before its enclosing
    // attachment separator or terminal. Trailing values are retained as
    // unconsumed bytes because the native reader only reads its own fields.
    [[nodiscard]] static ParamAnimComponentPayloadState read(
        std::span<const std::byte> input) {
        ParamAnimComponentPayloadState state;
        Cursor cursor(input);
        if (cursor.empty_or_terminal())
            return state;

        state.control_b = read_boolean(cursor);
        if (cursor.peek_class() != 2U) {
            state.control_a = state.control_b;
            state.control_b = read_boolean(cursor);
        } else {
            state.control_a = false;
        }

        state.scalar_c = read_float(cursor);
        state.initial_control = read_boolean(cursor);

        if (cursor.peek_class() == 10U)
            state.mode_word = static_cast<std::uint16_t>(read_integer(cursor));

        if (cursor.peek_class() == 2U) {
            state.scalar_a = read_float(cursor) / 25.0F;
            state.scalar_b = read_float(cursor) / 25.0F;
        }

        state.consumed_bytes = cursor.position();
        state.consumed_values = cursor.values_read();
        return state;
    }

private:
    class Cursor final {
    public:
        explicit Cursor(std::span<const std::byte> input) : input_(input) {}

        [[nodiscard]] bool empty_or_terminal() const noexcept {
            return position_ == input_.size() ||
                   std::to_integer<std::uint8_t>(input_[position_]) == 0xffU;
        }

        [[nodiscard]] std::uint8_t peek_class() const noexcept {
            if (empty_or_terminal())
                return 0x3fU;
            return static_cast<std::uint8_t>(
                std::to_integer<std::uint8_t>(input_[position_]) & 0x3fU);
        }

        [[nodiscard]] CompactTypedValue next() {
            if (empty_or_terminal())
                fail("ParamAnim payload is truncated before a required value");
            const auto remaining = input_.subspan(position_);
            if (peek_class() == 12U)
                fail("ParamAnim required field has no consuming advance for class twelve");
            CompactTypedValueDecoder decoder(remaining);
            auto value = decoder.next();
            if (value.kind == CompactTypedValueKind::terminator ||
                value.value_class == 6U || value.encoded.empty()) {
                fail("ParamAnim payload contains a separator inside a component slice");
            }
            position_ += value.encoded.size();
            ++values_read_;
            return value;
        }

        [[nodiscard]] std::size_t position() const noexcept { return position_; }
        [[nodiscard]] std::size_t values_read() const noexcept { return values_read_; }

    private:
        [[noreturn]] static void fail(const char* message) {
            throw std::runtime_error(message);
        }

        std::span<const std::byte> input_;
        std::size_t position_{};
        std::size_t values_read_{};
    };

    [[nodiscard]] static std::int32_t truncate_to_i32(double value) noexcept {
        // x86 CVTTSD2SI/CVTTSS2SI produce INT_MIN for NaN or out-of-range input.
        constexpr auto minimum = static_cast<double>(std::numeric_limits<std::int32_t>::min());
        constexpr auto upper_exclusive = 2147483648.0;
        if (!std::isfinite(value) || value < minimum || value >= upper_exclusive)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(std::trunc(value));
    }

    [[nodiscard]] static std::int32_t read_integer(Cursor& cursor) {
        const auto value = cursor.next();
        switch (value.kind) {
        case CompactTypedValueKind::binary64:
            return truncate_to_i32(std::bit_cast<double>(value.binary64_bits()));
        case CompactTypedValueKind::binary32:
            return truncate_to_i32(static_cast<double>(
                std::bit_cast<float>(value.binary32_bits())));
        case CompactTypedValueKind::signed32:
            return value.signed32();
        case CompactTypedValueKind::nul_terminated_string:
        case CompactTypedValueKind::length_u32:
            // The native numeric helper returns zero for nonnumeric classes,
            // while its paired cursor helper still advances over the value.
            return 0;
        case CompactTypedValueKind::terminator:
            break;
        }
        throw std::runtime_error("ParamAnim integer conversion has no supported value");
    }

    [[nodiscard]] static float read_float(Cursor& cursor) {
        const auto value = cursor.next();
        switch (value.kind) {
        case CompactTypedValueKind::binary64:
            return static_cast<float>(std::bit_cast<double>(value.binary64_bits()));
        case CompactTypedValueKind::binary32:
            return std::bit_cast<float>(value.binary32_bits());
        case CompactTypedValueKind::signed32:
            // Float fields interpret integer-class payload bits as unsigned;
            // integer and boolean fields retain their signed conversion.
            return static_cast<float>(value.u32_bits());
        case CompactTypedValueKind::nul_terminated_string:
        case CompactTypedValueKind::length_u32:
            return 0.0F;
        case CompactTypedValueKind::terminator:
            break;
        }
        throw std::runtime_error("ParamAnim float conversion has no supported value");
    }

    [[nodiscard]] static bool read_boolean(Cursor& cursor) {
        return read_integer(cursor) != 0;
    }
};

} // namespace off::data
