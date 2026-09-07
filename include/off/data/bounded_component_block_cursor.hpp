#pragma once

#include "off/data/compact_typed_value_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// A mutable view given to one deferred attachment.  Its end is the end of the
// caller-supplied compact block, never the end of an enclosing source stream.
// The view owns its cursor, so consuming it cannot alter the block scanner.
class DeferredComponentAttachmentSnapshot final {
public:
    DeferredComponentAttachmentSnapshot() = default;

    [[nodiscard]] std::span<const std::byte> remaining() const { return remaining_; }

    void consume(std::size_t count) {
        if (count > remaining_.size()) {
            throw std::runtime_error("deferred component snapshot exceeds its compact block");
        }
        remaining_ = remaining_.subspan(count);
    }

private:
    friend class BoundedComponentBlockCursor;
    explicit DeferredComponentAttachmentSnapshot(std::span<const std::byte> input) : remaining_(input) {}

    std::span<const std::byte> remaining_;
};

// Scans one already-bounded compact block through an externally-owned cursor.
// Each attachment snapshots the current shared cursor before the scanner moves
// it through generic values to the next low-six-bit class-six delimiter.  The
// delimiter is then consumed from the shared scanner and the independently
// mutable snapshot is returned.  Thus a reader sees the input which preceded
// its delimiter, while its own consumption cannot alter the scan that finds
// the next attachment.  The 0xff terminator ends scanning but deliberately
// remains at the shared cursor.  This boundary does not interpret owner data,
// raw tails, or any attachment grammar.
class BoundedComponentBlockCursor final {
public:
    BoundedComponentBlockCursor(std::span<const std::byte>& shared_cursor, std::size_t block_extent)
        : shared_cursor_(shared_cursor) {
        if (block_extent > shared_cursor_.size()) {
            fail("deferred component block exceeds its supplied source boundary");
        }
        remaining_block_bytes_ = block_extent;
    }

    [[nodiscard]] bool next_attachment(DeferredComponentAttachmentSnapshot& snapshot) {
        if (remaining_block_bytes_ == 0U) {
            fail("deferred component block is truncated before its terminator");
        }
        if (std::to_integer<std::uint8_t>(shared_cursor_.front()) == terminal_tag) {
            return false;
        }

        const auto attachment_start = block_remaining();
        while (remaining_block_bytes_ != 0U) {
            const auto tag = std::to_integer<std::uint8_t>(shared_cursor_.front());
            if (tag == terminal_tag) {
                return false;
            }
            if ((tag & class_mask) == component_delimiter) {
                shared_cursor_ = shared_cursor_.subspan(1U);
                --remaining_block_bytes_;
                snapshot = DeferredComponentAttachmentSnapshot(attachment_start);
                return true;
            }

            CompactTypedValueDecoder decoder(block_remaining());
            const auto value = decoder.next();
            if (value.kind == CompactTypedValueKind::terminator) {
                fail("deferred component scanner lost its terminator boundary");
            }
            shared_cursor_ = shared_cursor_.subspan(value.encoded.size());
            remaining_block_bytes_ -= value.encoded.size();
        }
        fail("deferred component block is truncated before its terminator");
    }

    [[nodiscard]] std::span<const std::byte> remaining() const { return block_remaining(); }

private:
    [[nodiscard]] std::span<const std::byte> block_remaining() const {
        return shared_cursor_.first(remaining_block_bytes_);
    }

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }

    static constexpr std::uint8_t component_delimiter = 0x06U;
    static constexpr std::uint8_t terminal_tag = 0xffU;
    static constexpr std::uint8_t class_mask = 0x3fU;

    std::span<const std::byte>& shared_cursor_;
    std::size_t remaining_block_bytes_{};
};

} // namespace off::data
