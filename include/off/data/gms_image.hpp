#pragma once

#include "off/data/packed_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace off::data {

class GmsImage final {
public:
    [[nodiscard]] static GmsImage parse(PackedResource resource);

    [[nodiscard]] std::span<const std::byte> resolve_reference(
        std::uint32_t packed_reference,
        std::size_t size
    ) const;
    [[nodiscard]] std::size_t decoded_size() const noexcept {
        return resource_.payload().size();
    }

private:
    PackedResource resource_;
};

}  // namespace off::data
