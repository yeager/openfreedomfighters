#pragma once

#include "off/data/bounded_component_block_cursor.hpp"

namespace off::data {

enum class LightOwnerFamily { spotlight, square_spotlight, omnidirectional };

struct LightParamAnimationDeferredReaderResult final {
    bool owner_flag_enabled{};
    std::span<const std::byte> component_suffix;
    std::size_t component_extent{};
};

// Owner-prefix reader only. Publication requires successful attachment dispatch
// and terminal validation by the caller; this result is not an admission.
class LightParamAnimationDeferredReader final {
public:
    [[nodiscard]] static LightParamAnimationDeferredReaderResult read(
        std::span<const std::byte> block, LightOwnerFamily family) {
        if (family != LightOwnerFamily::spotlight &&
            family != LightOwnerFamily::square_spotlight &&
            family != LightOwnerFamily::omnidirectional)
            fail();
        if (block.size() < 4U) fail();
        std::uint32_t extent{};
        for (std::size_t i{}; i < 4U; ++i)
            extent |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[i])) << (8U*i);
        if (extent != block.size()) fail();
        auto cursor = block.subspan(4U);
        if (peek(cursor) == 1U) {
            numeric_float(cursor);
            numeric_float(cursor);
            if (peek(cursor) == 3U) {
                static_cast<void>(integer(cursor));
                numeric_float(cursor);
            } else {
                numeric_float(cursor);
                for (unsigned i{}; i < 3U; ++i) static_cast<void>(integer(cursor));
            }
        }
        static_cast<void>(integer(cursor));
        static_cast<void>(integer(cursor));
        const auto enabled = integer(cursor) != 0;
        scan(cursor);
        if (family == LightOwnerFamily::spotlight && peek(cursor) == 1U)
            for (unsigned i{}; i < 3U; ++i) numeric_float(cursor);
        scan(cursor);
        return {enabled, cursor, cursor.size()};
    }

private:
    [[nodiscard]] static std::uint8_t peek(std::span<const std::byte> cursor) {
        if (cursor.empty()) fail();
        if (cursor.front() == std::byte{0xff}) return 0x3fU;
        return std::to_integer<std::uint8_t>(cursor.front()) & 0x3fU;
    }
    [[nodiscard]] static CompactTypedValue next(std::span<const std::byte>& cursor) {
        CompactTypedValueDecoder decoder(cursor);
        const auto value = decoder.next();
        cursor = cursor.subspan(value.encoded.size());
        return value;
    }
    [[nodiscard]] static std::int32_t integer(std::span<const std::byte>& cursor) {
        const auto value = next(cursor);
        if (value.kind != CompactTypedValueKind::signed32) fail();
        return value.signed32();
    }
    static void numeric_float(std::span<const std::byte>& cursor) {
        const auto cls = peek(cursor);
        if (cls != 1U && cls != 2U && cls != 3U) fail();
        static_cast<void>(next(cursor));
    }
    static void scan(std::span<const std::byte>& cursor) {
        for (;;) {
            const auto cls = peek(cursor);
            if (cursor.front() == std::byte{0xff}) return;
            if (cls == 6U) {
                cursor = cursor.subspan(1U);
                return;
            }
            cursor = cursor.subspan(compact_delimiter_scan_extent(cursor));
        }
    }
    [[noreturn]] static void fail() {
        throw std::runtime_error("light owner deferred prefix is unsupported or malformed");
    }
};

} // namespace off::data
