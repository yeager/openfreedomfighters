#pragma once

#include "off/data/bounded_component_block_cursor.hpp"

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
    // For each attachment, captures the current compact cursor, scans generic
    // values until a raw class-six delimiter or the 0xff terminator, then
    // consumes that delimiter before dispatching the captured cursor. The
    // caller supplies readers in attachment order. As with the recovered
    // compact advance helpers, the high tag bit does not change generic class
    // handling.
    [[nodiscard]] static DeferredComponentDispatchResult dispatch(
        std::span<const std::byte> input,
        std::span<const DeferredComponentReader> attachment_readers) {
        auto shared_cursor = input;
        BoundedComponentBlockCursor cursor(shared_cursor, input.size());
        std::size_t reader_index = 0;

        DeferredComponentAttachmentSnapshot snapshot;
        while (cursor.next_attachment(snapshot)) {
            if (reader_index == attachment_readers.size()) {
                fail("deferred component stream has more delimiters than attachment readers");
            }
            auto child_cursor = snapshot.remaining();
            attachment_readers[reader_index++](child_cursor);
        }
        if (reader_index != attachment_readers.size()) {
            fail("deferred component stream ends before every attachment reader");
        }
        return {shared_cursor, reader_index};
    }

private:
    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
};

} // namespace off::data
