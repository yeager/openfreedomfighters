#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>

namespace off::data {

// Dispatches the component-reader entry boundaries recovered from a deferred
// compact stream.  This intentionally knows neither an owner prefix nor any
// component grammar.  Readers receive an independently-owned cursor view, so
// their consumption cannot change the scan that finds subsequent boundaries.
using DeferredComponentReader = std::function<void(std::span<const std::byte>&)>;

struct DeferredComponentDispatchResult final {
    // The original terminator remains at the beginning of this suffix.  It is
    // deliberately not consumed by the dispatcher.
    std::span<const std::byte> continuation;
    std::size_t dispatched_components{};
};

class DeferredComponentDispatcher final {
public:
    // Scans generic compact values until a raw class-six delimiter or the
    // 0xff terminator.  A delimiter advances exactly one byte and dispatches
    // the next attachment reader.  The caller supplies readers in attachment
    // order. As with the recovered compact advance helpers, the high tag bit
    // does not change generic class handling.
    [[nodiscard]] static DeferredComponentDispatchResult dispatch(
        std::span<const std::byte> input,
        std::span<const DeferredComponentReader> attachment_readers) {
        std::size_t cursor = 0;
        std::size_t reader_index = 0;

        while (cursor < input.size()) {
            const auto tag = std::to_integer<std::uint8_t>(input[cursor]);
            if (tag == terminal_tag) {
                if (reader_index != attachment_readers.size()) {
                    fail("deferred component stream ends before every attachment reader");
                }
                return {input.subspan(cursor), reader_index};
            }
            if ((tag & class_mask) == component_delimiter) {
                if (reader_index == attachment_readers.size()) {
                    fail("deferred component stream has more delimiters than attachment readers");
                }
                ++cursor;
                auto child_cursor = input.subspan(cursor);
                attachment_readers[reader_index++](child_cursor);
                continue;
            }

            CompactTypedValueDecoder decoder(input.subspan(cursor));
            const auto value = decoder.next();
            if (value.kind == CompactTypedValueKind::terminator) {
                fail("deferred component scanner lost its terminator boundary");
            }
            cursor += value.encoded.size();
        }
        fail("deferred component stream is truncated before its terminator");
    }

private:
    static constexpr std::uint8_t component_delimiter = 0x06U;
    static constexpr std::uint8_t terminal_tag = 0xffU;
    static constexpr std::uint8_t class_mask = 0x3fU;

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
};

} // namespace off::data
