#pragma once

#include "off/data/packed_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace off::data {

struct ZgfEntry {
    std::string name;
    std::uint32_t record_offset{0};
    std::uint32_t payload_offset{0};
    std::uint32_t payload_size{0};
};

class ZgfBundle final {
public:
    [[nodiscard]] static ZgfBundle parse(PackedResource resource);

    [[nodiscard]] std::span<const ZgfEntry> entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::span<const std::byte> entry_payload(std::size_t index) const;
    [[nodiscard]] std::size_t decoded_size() const noexcept {
        return resource_.payload().size();
    }

private:
    PackedResource resource_;
    std::vector<ZgfEntry> entries_;
};

}  // namespace off::data
