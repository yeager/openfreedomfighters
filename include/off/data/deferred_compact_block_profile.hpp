#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

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
                return result;
            }
            if ((tag & class_mask) == attachment_delimiter) {
                ++result.attachment_delimiters;
                mix(result.framing_digest, tag);
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

    static constexpr std::uint64_t fnv_prime = 1099511628211ULL;
    static constexpr std::uint8_t class_mask = 0x3fU;
    static constexpr std::uint8_t attachment_delimiter = 0x06U;
    static constexpr std::uint8_t terminal_tag = 0xffU;
};

} // namespace off::data
