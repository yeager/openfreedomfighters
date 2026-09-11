#pragma once

#include "off/data/deferred_compact_block_profile.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace off::data {

// The only reviewed attachment-free ZGROUP compact body. This is a framing
// gate, not a field parser: matching it grants no interpretation of values,
// child membership, flags, or lifecycle behavior.
class BasicGroupDeferredReaderShape final {
public:
    [[nodiscard]] static bool matches(std::span<const std::byte> body) {
        const auto profile=DeferredCompactBlockProfiler::profile(body);
        return profile.attachment_delimiters==1U && profile.encoded_values==5U &&
            profile.continuation_values==0U && profile.framing_notation==notation;
    }

private:
    static constexpr std::string_view notation{"i3f2i3i3i3|!"};
};

} // namespace off::data
