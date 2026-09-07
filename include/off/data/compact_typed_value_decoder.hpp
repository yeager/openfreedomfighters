#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// A bounded decoder for the compact value encodings whose byte-level framing
// has been recovered.  It is intentionally not a GMS object reader: callers
// retain ownership of substream boundaries and assign field meaning.
enum class CompactTypedValueKind : std::uint8_t {
    terminator,
    binary64,
    binary32,
    signed32,
    nul_terminated_string,
    length_u32,
};

struct CompactTypedValue final {
    CompactTypedValueKind kind{};
    std::uint8_t raw_tag{};
    // The low six tag bits are the recovered value class.  The high bit is
    // retained in raw_tag but deliberately has no effect on this classifier.
    std::uint8_t value_class{};
    bool continuation{};
    // This view includes the tag and exactly the bytes advanced by this
    // decoder.  It lets a schema-specific reader retain byte provenance.
    std::span<const std::byte> encoded;
    // This is encoded.subspan(1) for non-terminators and empty for the
    // terminator.  It contains no inferred interpretation.
    std::span<const std::byte> payload;

    [[nodiscard]] std::uint32_t u32_bits() const {
        if (payload.size() != sizeof(std::uint32_t)) fail();
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < payload.size(); ++index) {
            value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[index]))
                     << static_cast<unsigned int>(index * 8U);
        }
        return value;
    }

    [[nodiscard]] std::int32_t signed32() const {
        if (kind != CompactTypedValueKind::signed32) fail();
        return std::bit_cast<std::int32_t>(u32_bits());
    }
    [[nodiscard]] std::uint32_t binary32_bits() const {
        if (kind != CompactTypedValueKind::binary32) fail();
        return u32_bits();
    }
    [[nodiscard]] std::uint64_t binary64_bits() const {
        if (kind != CompactTypedValueKind::binary64 || payload.size() != sizeof(std::uint64_t)) {
            fail();
        }
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < payload.size(); ++index) {
            value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(payload[index]))
                     << static_cast<unsigned int>(index * 8U);
        }
        return value;
    }

private:
    [[noreturn]] static void fail() {
        throw std::runtime_error("compact typed value has no requested representation");
    }
};

class CompactTypedValueDecoder final {
public:
    explicit CompactTypedValueDecoder(std::span<const std::byte> input) : input_(input) {}

    [[nodiscard]] bool empty() const noexcept { return cursor_ == input_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept { return input_.size() - cursor_; }

    // Decodes one value without interpreting its schema role.  Unknown class
    // values and class 6 are rejected without advancement because their byte
    // extent is not recovered by the generic contract.
    [[nodiscard]] CompactTypedValue next() {
        if (empty()) fail("compact typed value stream is truncated");
        const auto begin = cursor_;
        const auto tag = std::to_integer<std::uint8_t>(input_[cursor_]);
        if (tag == terminal_tag) {
            ++cursor_;
            return {CompactTypedValueKind::terminator, tag, 0U, false,
                    input_.subspan(begin, 1U), {}};
        }

        const auto value_class = static_cast<std::uint8_t>(tag & class_mask);
        const auto continuation = (tag & continuation_mask) != 0U;
        const auto kind = classify(value_class);
        const auto payload_size = fixed_payload_size(kind);
        if (kind == CompactTypedValueKind::nul_terminated_string) {
            ++cursor_;
            while (cursor_ < input_.size() && input_[cursor_] != std::byte{0}) ++cursor_;
            if (cursor_ == input_.size()) fail("compact string value is unterminated");
            ++cursor_;
        } else {
            if (payload_size > input_.size() - cursor_ - 1U) {
                fail("compact typed value payload is truncated");
            }
            cursor_ += 1U + payload_size;
        }
        const auto encoded = input_.subspan(begin, cursor_ - begin);
        return {kind, tag, value_class, continuation, encoded, encoded.subspan(1U)};
    }

private:
    static constexpr std::uint8_t class_mask = 0x3fU;
    static constexpr std::uint8_t continuation_mask = 0x40U;
    static constexpr std::uint8_t terminal_tag = 0xffU;

    [[nodiscard]] static CompactTypedValueKind classify(std::uint8_t value_class) {
        switch (value_class) {
        case 1U: return CompactTypedValueKind::binary64;
        case 2U: return CompactTypedValueKind::binary32;
        case 3U:
        case 8U:
        case 10U:
        case 11U: return CompactTypedValueKind::signed32;
        case 4U:
        case 5U: return CompactTypedValueKind::nul_terminated_string;
        case 7U: return CompactTypedValueKind::length_u32;
        default: fail("compact typed value class has no recovered generic extent");
        }
    }

    [[nodiscard]] static std::size_t fixed_payload_size(CompactTypedValueKind kind) {
        switch (kind) {
        case CompactTypedValueKind::binary64: return sizeof(std::uint64_t);
        case CompactTypedValueKind::binary32:
        case CompactTypedValueKind::signed32:
        case CompactTypedValueKind::length_u32: return sizeof(std::uint32_t);
        case CompactTypedValueKind::terminator:
        case CompactTypedValueKind::nul_terminated_string: return 0U;
        }
        fail("compact typed value kind has no fixed payload size");
    }

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }

    std::span<const std::byte> input_;
    std::size_t cursor_{};
};

} // namespace off::data
