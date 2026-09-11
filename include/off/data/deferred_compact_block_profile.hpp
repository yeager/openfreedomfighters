#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace off::data {

// Structural evidence for an already-bounded deferred block body. It records
// framing only: no decoded payload values, attachment identity, owner fields,
// or component semantics escape this boundary. Class-six values are the
// recovered attachment delimiters; 0xff is the required final terminator.
struct DeferredCompactBlockProfile final {
    std::array<std::size_t, 6> value_kinds{};
    std::size_t attachment_delimiters{};
    std::size_t continuation_values{};
    std::size_t encoded_values{};
    // FNV-1a over framing tags only (including delimiters and the terminator).
    // It intentionally excludes every payload byte.
    std::uint64_t framing_digest{14695981039346656037ULL};
    // One symbol per framing element: d=64-bit, f=32-bit, i=signed word,
    // s=NUL string, l=length word, |=attachment delimiter, !=terminator.
    // A trailing ^ marks a raw high bit and + the continuation bit. Payload
    // bytes are excluded.
    std::string framing_notation;

    [[nodiscard]] bool operator==(const DeferredCompactBlockProfile&) const = default;
};

class DeferredCompactBlockProfiler final {
public:
    [[nodiscard]] static DeferredCompactBlockProfile profile(std::span<const std::byte> body) {
        if (body.empty()) fail();
        DeferredCompactBlockProfile result;
        auto remaining = body;
        while (!remaining.empty()) {
            const auto tag = std::to_integer<std::uint8_t>(remaining.front());
            if (tag == terminal_tag) {
                if (remaining.size() != 1U) fail();
                mix(result.framing_digest, tag);
                result.framing_notation.push_back('!');
                return result;
            }
            if ((tag & class_mask) == attachment_delimiter) {
                ++result.attachment_delimiters;
                mix(result.framing_digest, tag);
                result.framing_notation.push_back('|');
                remaining = remaining.subspan(1U);
                continue;
            }
            CompactTypedValueDecoder decoder(remaining);
            const auto value = decoder.next();
            if (value.kind == CompactTypedValueKind::terminator || value.encoded.empty()) fail();
            const auto index = static_cast<std::size_t>(value.kind);
            if (index >= result.value_kinds.size() || value.encoded.size() > remaining.size()) fail();
            ++result.value_kinds[index];
            ++result.encoded_values;
            if (value.continuation) ++result.continuation_values;
            mix(result.framing_digest, value.raw_tag);
            result.framing_notation.push_back(symbol(value.kind));
            if ((value.raw_tag & 0x80U) != 0U) result.framing_notation.push_back('^');
            if (value.continuation) result.framing_notation.push_back('+');
            remaining = remaining.subspan(value.encoded.size());
        }
        fail();
    }

private:
    [[noreturn]] static void fail() {
        throw std::runtime_error("deferred compact block profile is malformed or has no final terminator");
    }
    static void mix(std::uint64_t& state, std::uint8_t tag) noexcept {
        state ^= tag;
        state *= fnv_prime;
    }
    [[nodiscard]] static char symbol(CompactTypedValueKind kind) {
        switch (kind) {
        case CompactTypedValueKind::binary64: return 'd';
        case CompactTypedValueKind::binary32: return 'f';
        case CompactTypedValueKind::signed32: return 'i';
        case CompactTypedValueKind::nul_terminated_string: return 's';
        case CompactTypedValueKind::length_u32: return 'l';
        case CompactTypedValueKind::terminator: break;
        }
        fail();
    }

    static constexpr std::uint64_t fnv_prime = 1099511628211ULL;
    static constexpr std::uint8_t class_mask = 0x3fU;
    static constexpr std::uint8_t attachment_delimiter = 0x06U;
    static constexpr std::uint8_t terminal_tag = 0xffU;
};

} // namespace off::data
