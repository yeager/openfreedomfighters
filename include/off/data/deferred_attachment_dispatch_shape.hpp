#pragma once

#include "off/data/bounded_component_block_cursor.hpp"

#include <cstddef>
#include <span>

namespace off::data {

// A structural observation of one already-bounded deferred compact block. It
// deliberately does not interpret owner fields or invoke attachment readers.
enum class DeferredAttachmentDispatchShape {
  terminal_before_first_attachment_delimiter,
  attachment_delimiter_precedes_terminal,
};

struct DeferredAttachmentDispatchObservation final {
  DeferredAttachmentDispatchShape shape{};
  std::size_t delimiter_count{};
};

class DeferredAttachmentDispatchClassifier final {
public:
  [[nodiscard]] static DeferredAttachmentDispatchObservation observe(
      std::span<const std::byte> compact_block) {
    auto shared = compact_block;
    BoundedComponentBlockCursor cursor(shared, compact_block.size());
    DeferredComponentAttachmentSnapshot ignored;
    std::size_t delimiters{};
    while (cursor.next_attachment(ignored)) ++delimiters;
    return {delimiters == 0U
                ? DeferredAttachmentDispatchShape::terminal_before_first_attachment_delimiter
                : DeferredAttachmentDispatchShape::attachment_delimiter_precedes_terminal,
            delimiters};
  }
};

} // namespace off::data
