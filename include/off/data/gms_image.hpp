#pragma once

#include "off/data/packed_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace off::data {

struct GmsDirectoryEntry {
    std::uint32_t packed_record_reference{0};
    std::uint32_t auxiliary_value{0};
    std::uint32_t record_offset{0};
    std::uint8_t hierarchy_depth{0};
    bool flagged{false};
};

struct GmsObjectHandle {
    std::uint32_t byte_offset{0};
    std::uint32_t slot_index{0};
};

class GmsImage final {
public:
    [[nodiscard]] static GmsImage parse(PackedResource resource);
    [[nodiscard]] static GmsObjectHandle decode_object_handle(
        std::uint32_t packed_reference
    );

    [[nodiscard]] const std::vector<GmsDirectoryEntry>& directory() const noexcept {
        return directory_;
    }
    [[nodiscard]] std::size_t identifier_count() const noexcept {
        return identifier_count_;
    }
    [[nodiscard]] std::size_t decoded_size() const noexcept {
        return resource_.payload().size();
    }

private:
    PackedResource resource_;
    std::vector<GmsDirectoryEntry> directory_;
    std::size_t identifier_count_{0};
};

}  // namespace off::data
