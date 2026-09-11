#pragma once

#include <cstddef>
#include <span>

namespace off::data {

// A cheap routing gate for the exact record parsed by BasicGroupDeferredReader.
// It retains no values and must never be used as an admission on its own.
class BasicGroupDeferredReaderShape final {
public:
    [[nodiscard]] static bool matches(std::span<const std::byte> body) {
        if (body.size()!=27U || byte(body,25U)!=0x06U || byte(body,26U)!=0xffU) return false;
        return tag(body,0U,3U,true) && tag(body,5U,2U,false) && tag(body,10U,3U,false) &&
            tag(body,15U,3U,false) && tag(body,20U,3U,false);
    }

private:
    [[nodiscard]] static std::uint8_t byte(std::span<const std::byte> body,std::size_t offset) {
        return std::to_integer<std::uint8_t>(body[offset]);
    }
    [[nodiscard]] static bool tag(std::span<const std::byte> body,std::size_t offset,
                                  std::uint8_t expected,bool allow_high_bit) {
        const auto raw=byte(body,offset);
        return (raw&0x40U)==0U && (raw&0x3fU)==expected && (allow_high_bit || (raw&0x80U)==0U);
    }
};

} // namespace off::data
